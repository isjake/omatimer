// omatimer - a native Qt6 rewrite of papertimer.
// Type a duration, hit Enter/Space to start or stop, R to reset, B to swap
// between the dark and light background, Ctrl+M to mute.

#include <QApplication>
#include <QWidget>
#include <QAbstractButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QDateTime>
#include <QSettings>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QHash>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QStyle>
#include <QStyleOption>
#include <QStandardPaths>
#include <QProcess>
#include <QFileSystemWatcher>

static const char *kCompleteSound =
    "/usr/share/sounds/freedesktop/stereo/complete.oga";
static const char *kBellSound =
    "/usr/share/sounds/freedesktop/stereo/message.oga";

// The face omacalc and omawrite use, so the three apps look related.
static QString uiFontFamily()
{
    for (const char *want : {"iA Writer Mono S", "iA Writer Duospace"})
        if (QFontDatabase::families().contains(QString::fromUtf8(want)))
            return QString::fromUtf8(want);
    return QStringLiteral("monospace");
}

// omacalc builds every surface by mixing ink into the page rather than
// hardcoding greys, which is what keeps it in step with the theme.
static QColor mix(const QColor &base, const QColor &tint, qreal amount)
{
    return QColor::fromRgbF(base.redF() + (tint.redF() - base.redF()) * amount,
                            base.greenF() + (tint.greenF() - base.greenF()) * amount,
                            base.blueF() + (tint.blueF() - base.blueF()) * amount);
}

// "90" -> 90, "1h 30m 10s" -> 5410, "1.5m" -> 90
static int parseDurationToSeconds(const QString &input)
{
    const QString s = input.trimmed();
    if (s.isEmpty())
        return 0;

    static const QRegularExpression plain("^\\d+$");
    if (plain.match(s).hasMatch())
        return s.toInt();

    static const QRegularExpression unit("(\\d+(?:\\.\\d+)?)\\s*([hms])",
                                         QRegularExpression::CaseInsensitiveOption);
    double total = 0;
    auto it = unit.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        const double value = m.captured(1).toDouble();
        const QChar u = m.captured(2).toLower().at(0);
        if (u == 'h') total += value * 3600;
        else if (u == 'm') total += value * 60;
        else total += value;
    }
    return static_cast<int>(total);
}

// 90 -> "1m30s", 3661 -> "1h1m1s", 45 -> "45s". Round-trips back through
// parseDurationToSeconds, so the box stays editable in its own notation.
static QString formatDuration(int seconds)
{
    if (seconds < 60)
        return QString::number(seconds) + "s";

    const int h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
    QString out;
    if (h) out += QString::number(h) + "h";
    if (m) out += QString::number(m) + "m";
    if (s) out += QString::number(s) + "s";
    return out;
}

static QString clockString(const QDateTime &t)
{
    return t.time().toString("h:mm:ss");
}

// Icons are drawn, not typed: the glyphs for these shapes are missing from
// plenty of monospace faces and render as tofu when they are.
class IconButton : public QAbstractButton
{
public:
    enum Kind { Play, Pause, Reset, Contrast };

    IconButton(Kind kind, QWidget *parent = nullptr)
        : QAbstractButton(parent), kind(kind)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
    }

    void setKind(Kind k) { kind = k; update(); }
    void setColors(const QColor &p, const QColor &i)
    {
        page = p;
        ink = i;
        update();
    }
    void setChrome(bool on) { chrome = on; update(); } // false = bare glyph

protected:
    QSize sizeHint() const override { return QSize(64, 64); }

    void paintEvent(QPaintEvent *) override
    {
        // Resting lift, plus a little more under the pointer and more again
        // while held - the same three-step feedback omacalc uses.
        qreal lift = chrome ? 0.05 : 0.0;
        if (underMouse()) lift += 0.045;
        if (isDown()) lift += 0.09;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        if (chrome) {
            const qreal radius = qMin(14.0, height() * 0.18);
            p.setPen(QPen(mix(page, ink, 0.13), 1.0));
            p.setBrush(mix(page, ink, lift));
            p.drawRoundedRect(r, radius, radius);
        } else if (lift > 0.0) {
            p.setPen(Qt::NoPen);
            p.setBrush(mix(page, ink, lift));
            p.drawEllipse(r);
        }

        const qreal s = qMin(width(), height()) * (chrome ? 0.36 : 0.52);
        const QPointF c = r.center();
        p.setPen(Qt::NoPen);
        p.setBrush(ink);

        switch (kind) {
        case Play: {
            // Nudged right so the triangle looks optically centred.
            QPainterPath path;
            path.moveTo(c.x() - s * 0.42 + s * 0.12, c.y() - s * 0.58);
            path.lineTo(c.x() + s * 0.62 + s * 0.12, c.y());
            path.lineTo(c.x() - s * 0.42 + s * 0.12, c.y() + s * 0.58);
            path.closeSubpath();
            p.drawPath(path);
            break;
        }
        case Pause: {
            const qreal w = s * 0.24, gap = s * 0.22, h = s * 0.58;
            p.drawRoundedRect(QRectF(c.x() - gap - w, c.y() - h, w, h * 2), w * 0.35, w * 0.35);
            p.drawRoundedRect(QRectF(c.x() + gap, c.y() - h, w, h * 2), w * 0.35, w * 0.35);
            break;
        }
        case Reset: {
            // An arc with an arrowhead, rather than a glyph.
            const qreal rad = s * 0.52;
            const qreal thick = qMax(1.6, s * 0.17);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(ink, thick, Qt::SolidLine, Qt::RoundCap));
            const QRectF arc(c.x() - rad, c.y() - rad, rad * 2, rad * 2);
            p.drawArc(arc, 70 * 16, 280 * 16);
            p.setPen(Qt::NoPen);
            p.setBrush(ink);
            const QPointF tip(c.x() + rad * qCos(qDegreesToRadians(70.0)),
                              c.y() - rad * qSin(qDegreesToRadians(70.0)));
            QPainterPath head;
            head.moveTo(tip.x() - thick * 1.5, tip.y() - thick * 0.2);
            head.lineTo(tip.x() + thick * 1.5, tip.y() - thick * 0.2);
            head.lineTo(tip.x(), tip.y() + thick * 1.9);
            head.closeSubpath();
            p.drawPath(head);
            break;
        }
        case Contrast: {
            // Half-filled circle: the usual mark for swapping light and dark.
            const qreal rad = s * 0.5;
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(ink, qMax(1.2, s * 0.11)));
            p.drawEllipse(c, rad, rad);
            p.setPen(Qt::NoPen);
            p.setBrush(ink);
            QPainterPath half;
            half.moveTo(c.x(), c.y() - rad);
            half.arcTo(QRectF(c.x() - rad, c.y() - rad, rad * 2, rad * 2), 90, 180);
            half.closeSubpath();
            p.drawPath(half);
            break;
        }
        }
    }

private:
    Kind kind;
    bool chrome = true;
    QColor page = Qt::black, ink = Qt::white;
};

// A hairline that fills as the timer runs. Hidden when nothing is counting.
class ProgressLine : public QWidget
{
public:
    explicit ProgressLine(QWidget *parent = nullptr) : QWidget(parent) {}

    void setColors(const QColor &p, const QColor &i, const QColor &a)
    {
        page = p;
        ink = i;
        accent = a;
        update();
    }
    void setFraction(qreal f)
    {
        fraction = qBound(0.0, f, 1.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal r = height() / 2.0;
        p.setPen(Qt::NoPen);
        p.setBrush(mix(page, ink, 0.12));
        p.drawRoundedRect(rect(), r, r);
        if (fraction <= 0.0)
            return;
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(0, 0, width() * fraction, height()), r, r);
    }

private:
    qreal fraction = 0.0;
    QColor page = Qt::black, ink = Qt::white, accent = Qt::white;
};

class Omatimer : public QWidget
{
public:
    explicit Omatimer(const QString &preset = QString(), bool autostart = false)
    {
        setWindowTitle("Omatimer");
        setMinimumSize(280, 220);
        resize(800, 600);
        setObjectName("root");

        const QString family = uiFontFamily();

        contrast = new IconButton(IconButton::Contrast);
        contrast->setChrome(false);

        input = new QLineEdit;
        input->setPlaceholderText("60s");
        input->setAlignment(Qt::AlignCenter);
        input->setFrame(false);
        QFont mono(family);
        input->setFont(mono);
        input->installEventFilter(this);
        connect(input, &QLineEdit::textChanged, this, [this] { fitInput(); });

        progress = new ProgressLine;

        // One line instead of two: less furniture around the number.
        schedule = new QLabel(idleSchedule());
        schedule->setAlignment(Qt::AlignCenter);

        playPause = new IconButton(IconButton::Play);
        reset = new IconButton(IconButton::Reset);

        schedule->setFont(QFont(family));

        // A spacer the width of the contrast mark keeps play/reset centred
        // while the mark sits off to the right.
        balance = new QWidget;
        auto *buttons = new QHBoxLayout;
        buttons->addWidget(balance);
        buttons->addStretch();
        buttons->addWidget(playPause);
        buttons->addWidget(reset);
        buttons->addStretch();
        buttons->addWidget(contrast);

        auto *bar = new QHBoxLayout;
        bar->addStretch(1);
        bar->addWidget(progress, 6);
        bar->addStretch(1);

        auto *root = new QVBoxLayout(this);
        root->addStretch(3);
        root->addWidget(input);
        root->addLayout(bar);
        root->addWidget(schedule);
        root->addLayout(buttons);
        root->addStretch(3);

        ticker = new QTimer(this);
        ticker->setSingleShot(true);
        ticker->setTimerType(Qt::PreciseTimer);
        connect(ticker, &QTimer::timeout, this, &Omatimer::tick);

        connect(playPause, &QAbstractButton::clicked, this, [this] {
            play(kBellSound);
            startOrStop();
        });
        connect(reset, &QAbstractButton::clicked, this, [this] {
            play(kBellSound);
            resetTimer();
        });
        connect(contrast, &QAbstractButton::clicked, this, [this] { swapBackground(); });

        QSettings s;
        savedSeconds = s.value("savedSeconds", 60).toInt();
        loadOmarchyColors();
        overridden = s.value("backgroundOverridden", false).toBool();
        theme = overridden ? s.value("theme", 0).toInt() % 2
                           : (themeIsLight ? 1 : 0);

        // Re-tint live when the Omarchy theme changes, the way omacalc does.
        watcher = new QFileSystemWatcher(this);
        watcher->addPath(omarchyColorsPath());
        connect(watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &f) {
            loadOmarchyColors();
            // Picking a new theme is a fresh instruction, so it overrides an
            // earlier B press rather than being ignored by it.
            overridden = false;
            theme = themeIsLight ? 1 : 0;
            applyTheme();
            if (!watcher->files().contains(f))
                watcher->addPath(f); // themes replace the file, not edit it
        });

        applyTheme();
        if (!preset.isEmpty()) {
            const int seconds = parseDurationToSeconds(preset);
            if (seconds > 0) {
                savedSeconds = seconds;
                activeSeconds = seconds;
            }
        }
        activeSeconds = savedSeconds;
        input->setText(formatDuration(savedSeconds));
        input->setFocus();

        if (autostart)
            QTimer::singleShot(0, this, [this] { startOrStop(); });
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (!handleShortcut(event))
            QWidget::keyPressEvent(event);
    }

    // The duration box keeps the focus, so shortcuts are pulled out of its
    // key stream before it turns them into text.
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == input && event->type() == QEvent::KeyPress
            && handleShortcut(static_cast<QKeyEvent *>(event)))
            return true;
        return QWidget::eventFilter(watched, event);
    }

    // A plain QWidget subclass ignores a stylesheet background unless it
    // draws PE_Widget itself.
    void paintEvent(QPaintEvent *) override
    {
        QStyleOption opt;
        opt.initFrom(this);
        QPainter p(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    }

    // Everything scales off one unit derived from both dimensions, so the
    // layout keeps its proportions in a short-wide or tall-narrow window.
    void resizeEvent(QResizeEvent *event) override
    {
        const qreal u = qMin(width() / 34.0, height() / 22.0);
        const QString family = uiFontFamily();

        QFont small(family);
        small.setPointSizeF(qBound(8.0, u * 0.95, 34.0));
        schedule->setFont(small);

        bigCap = qBound(20.0, u * 4.2, 220.0);
        fitInput(true); // the window changed size, so re-measure regardless

        const int hit = qRound(qBound(40.0, u * 3.6, 150.0));
        playPause->setFixedSize(hit, hit);
        reset->setFixedSize(hit, hit);
        const int mark = qRound(qBound(18.0, u * 1.6, 60.0));
        contrast->setFixedSize(mark, mark);
        balance->setFixedSize(mark, 1);

        progress->setFixedHeight(qRound(qBound(3.0, u * 0.22, 10.0)));

        if (auto *l = qobject_cast<QVBoxLayout *>(layout()))
            l->setSpacing(qRound(u * 0.7));

        QWidget::resizeEvent(event);
    }

    void closeEvent(QCloseEvent *event) override
    {
        QSettings s;
        s.setValue("theme", theme);
        s.setValue("backgroundOverridden", overridden);
        s.setValue("savedSeconds", savedSeconds);
        QWidget::closeEvent(event);
    }

private:
    // "1h5m30s" is several times wider than "60", so the display shrinks to
    // fit rather than clipping the tail off the duration.
    void fitInput(bool force = false)
    {
        const QString text = input->text().isEmpty() ? input->placeholderText()
                                                     : input->text();
        // "1m57s" -> "1m56s" is the same width, and re-measuring then
        // re-setting a 100pt font every second relaid out the whole window
        // for nothing. Only a change in length can change the fit.
        if (!force && text.length() == fittedLength)
            return;
        fittedLength = text.length();

        const QString family = uiFontFamily();
        const qreal avail = qMax(1, input->width() - 16);
        qreal size = bigCap;
        QFont f(family);
        f.setPointSizeF(size);
        const qreal w = QFontMetricsF(f).horizontalAdvance(text);
        if (w > avail)
            size = qMax(12.0, size * avail / w);
        f.setPointSizeF(size);
        if (input->font().pointSizeF() != size)
            input->setFont(f);
    }

    static QString idleSchedule() { return QStringLiteral("--:--:--  ·  --:--:--"); }

    // Returns true when the key was a shortcut and should not become text.
    bool handleShortcut(QKeyEvent *event)
    {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            play(kBellSound);
            startOrStop();
            return true;
        case Qt::Key_R:
            play(kBellSound);
            resetTimer();
            return true;
        case Qt::Key_B:
            swapBackground();
            return true;
        case Qt::Key_M:
            if (!(event->modifiers() & Qt::ControlModifier))
                return false; // plain "m" is part of "25m"
            muted = !muted;
            setWindowTitle(muted ? "Omatimer (muted)" : "Omatimer");
            return true;
        case Qt::Key_Escape:
            close();
            return true;
        default:
            return false;
        }
    }

    void startOrStop()
    {
        running = !running;
        playPause->setKind(running ? IconButton::Pause : IconButton::Play);
        if (!running) {
            ticker->stop();
            return;
        }

        // A fresh (or finished) timer picks up whatever is in the box.
        if (activeSeconds == savedSeconds || activeSeconds <= 0) {
            const int seconds = parseDurationToSeconds(input->text());
            if (seconds > 0) {
                activeSeconds = seconds;
                savedSeconds = seconds;
            } else {
                activeSeconds = savedSeconds;
            }
        }

        runSeconds = activeSeconds;
        deadline = QDateTime::currentDateTime().addSecs(activeSeconds);
        schedule->setText(clockString(QDateTime::currentDateTime()) + "  ·  "
                          + clockString(deadline));
        input->setText(formatDuration(activeSeconds));
        progress->setFraction(0.0);
        scheduleTick();
    }

    // Wake exactly when the displayed figure is due to change, rather than
    // every 1000ms from whenever the timer happened to start. A fixed
    // interval drifts off the second boundary, and once it does the display
    // shows the same figure twice or skips one entirely.
    void scheduleTick()
    {
        const qint64 ms = QDateTime::currentDateTime().msecsTo(deadline);
        if (ms <= 0) {
            tick();
            return;
        }
        // The figure is a ceiling, so it changes as the remainder hits zero.
        int delay = static_cast<int>(ms % 1000);
        if (delay == 0)
            delay = 1000;
        ticker->start(delay + 4); // a hair past the boundary, never before it
    }

    // Count down against a wall-clock deadline so a slow tick can't drift.
    void tick()
    {
        const qint64 ms = QDateTime::currentDateTime().msecsTo(deadline);
        // Ceiling: a timer reads 1s until the moment it is actually up.
        activeSeconds = ms > 0 ? static_cast<int>((ms + 999) / 1000) : 0;

        input->setText(formatDuration(activeSeconds));
        if (runSeconds > 0)
            progress->setFraction(1.0 - qreal(activeSeconds) / runSeconds);

        if (activeSeconds <= 0) {
            ticker->stop();
            running = false;
            playPause->setKind(IconButton::Play);
            progress->setFraction(1.0);
            schedule->setText(QString("done  ·  %1").arg(formatDuration(savedSeconds)));
            input->setText(formatDuration(savedSeconds));
            play(kCompleteSound);
            return;
        }
        scheduleTick();
    }

    void resetTimer()
    {
        ticker->stop();
        running = false;
        playPause->setKind(IconButton::Play);
        activeSeconds = savedSeconds;
        input->setText(formatDuration(savedSeconds));
        schedule->setText(idleSchedule());
        progress->setFraction(0.0);
    }

    void swapBackground()
    {
        theme = 1 - theme;
        // Back to following the theme once B lands on what it asked for.
        overridden = theme != (themeIsLight ? 1 : 0);
        applyTheme();
    }

    static QString omarchyColorsPath()
    {
        return QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml";
    }

    // Same file omacalc and omawrite read, so all three match the desktop.
    void loadOmarchyColors()
    {
        QColor bg("#1a1a1a"), fg("#cccccc"), light("#e8e8e8");
        accent = QColor("#7aa2f7");
        themeIsLight = false;

        QFile f(omarchyColorsPath());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            static const QRegularExpression entry(
                "^\\s*(\\w+)\\s*=\\s*\"([^\"]+)\"");
            QHash<QString, QString> c;
            while (!f.atEnd()) {
                const auto m = entry.match(QString::fromUtf8(f.readLine()));
                if (m.hasMatch())
                    c.insert(m.captured(1), m.captured(2));
            }
            themeIsLight = c.value("mode").compare("light", Qt::CaseInsensitive) == 0;
            if (c.contains("background")) {
                bg = QColor(c.value("background"));
                fg = QColor(c.value("foreground", fg.name()));
                light = QColor(c.value("bright_foreground",
                                       c.value("light_foreground", fg.name())));
                accent = QColor(c.value("accent", accent.name()));
            }
        }

        // The theme gives one pair; the other is it turned inside out. Sort by
        // lightness so index 0 is always the dark one whichever mode we're in.
        QPair<QColor, QColor> a(bg, fg), b(light, bg);
        if (a.first.lightnessF() > b.first.lightnessF())
            qSwap(a, b);
        pages[0] = a.first;
        inks[0] = a.second;
        pages[1] = b.first;
        inks[1] = b.second;
    }

    void applyTheme()
    {
        const QColor page = pages[theme], ink = inks[theme];
        for (IconButton *b : {playPause, reset, contrast})
            b->setColors(page, ink);
        progress->setColors(page, ink, accent);

        // Secondary text sits back from the number instead of competing.
        const QColor quiet = mix(page, ink, 0.55);
        setStyleSheet(QString("#root { background: %1; }"
                              "QLineEdit { background: transparent; color: %2;"
                              "  border: none; selection-background-color: %3;"
                              "  selection-color: %1; }"
                              "QLabel { background: transparent; color: %4; }")
                          .arg(page.name(), ink.name(), accent.name(), quiet.name()));
    }

    // Qt Multimedia would drag in FFmpeg, VA-API and GTK just to play a
    // one-second chime, so hand the file to PipeWire and forget about it.
    void play(const QString &path)
    {
        if (muted || !QFileInfo::exists(path))
            return;
        static const QString player =
            QStandardPaths::findExecutable("pw-play").isEmpty()
                ? QStandardPaths::findExecutable("paplay")
                : QStandardPaths::findExecutable("pw-play");
        if (!player.isEmpty())
            QProcess::startDetached(player, {path});
    }

    QLabel *schedule;
    QWidget *balance;
    QLineEdit *input;
    IconButton *playPause, *reset, *contrast;
    ProgressLine *progress;
    QTimer *ticker;
    QFileSystemWatcher *watcher = nullptr;
    QDateTime deadline;
    QColor pages[2], inks[2], accent;
    qreal bigCap = 60.0;
    int fittedLength = -1;
    int activeSeconds = 60, savedSeconds = 60, runSeconds = 60, theme = 0;
    bool running = false, muted = false;
    bool themeIsLight = false, overridden = false;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("omarchy");
    QCoreApplication::setApplicationName("omatimer");
    const QStringList args = QCoreApplication::arguments();
    QString preset;
    bool autostart = false;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--start" || args[i] == "-s")
            autostart = true;
        else if (!args[i].startsWith('-'))
            preset += args[i];
    }

    Omatimer w(preset, autostart);
    w.show();
    return app.exec();
}
