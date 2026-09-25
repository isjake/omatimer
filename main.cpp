// omatimer - a native Qt6 rewrite of papertimer.
// Same behaviour as the Electron version: type a duration, hit Enter/Space to
// start or stop, R to reset, M to mute, B to cycle the background.

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QDateTime>
#include <QSettings>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFileInfo>
#include <QFont>
#include <QResizeEvent>

static const char *kCompleteSound =
    "/usr/share/sounds/freedesktop/stereo/complete.oga";
static const char *kBellSound =
    "/usr/share/sounds/freedesktop/stereo/message.oga";

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

static QString clockString(const QDateTime &t)
{
    return t.time().toString("h:mm:ss");
}

class Omatimer : public QWidget
{
public:
    explicit Omatimer(const QString &preset = QString(), bool autostart = false)
    {
        setWindowTitle("Omatimer");
        setMinimumSize(280, 220);
        resize(800, 600);

        conversions = new QLabel("0.0min = 0.00hr");
        background = new QPushButton(QString::fromUtf8("☀"));
        background->setFlat(true);
        background->setCursor(Qt::PointingHandCursor);
        background->setFocusPolicy(Qt::NoFocus);

        input = new QLineEdit;
        input->setPlaceholderText("60s");
        input->setAlignment(Qt::AlignCenter);
        QFont big = input->font();
        big.setFamily("monospace");
        input->setFont(big);
        input->setFrame(false);

        started = new QLabel("Started: --:--:--");
        ends = new QLabel("Ends at: --:--:--");
        started->setAlignment(Qt::AlignCenter);
        ends->setAlignment(Qt::AlignCenter);

        playPause = new QPushButton(QString::fromUtf8("▶"));
        reset = new QPushButton(QString::fromUtf8("↺"));
        for (QPushButton *b : {playPause, reset}) {
            QFont f = b->font();
            f.setPointSize(28);
            b->setFont(f);
            b->setFlat(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFocusPolicy(Qt::NoFocus);
        }

        auto *top = new QHBoxLayout;
        top->addStretch();
        top->addWidget(conversions);
        top->addWidget(background);
        top->addStretch();

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        buttons->addWidget(playPause);
        buttons->addWidget(reset);
        buttons->addStretch();

        auto *root = new QVBoxLayout(this);
        root->addStretch();
        root->addLayout(top);
        root->addWidget(input);
        root->addWidget(started);
        root->addWidget(ends);
        root->addLayout(buttons);
        root->addStretch();

        // One shared player is enough; the two sounds never overlap.
        audio = new QAudioOutput(this);
        player = new QMediaPlayer(this);
        player->setAudioOutput(audio);

        ticker = new QTimer(this);
        ticker->setInterval(1000);
        ticker->setTimerType(Qt::PreciseTimer);
        connect(ticker, &QTimer::timeout, this, &Omatimer::tick);

        connect(playPause, &QPushButton::clicked, this, [this] {
            play(kBellSound);
            startOrStop();
        });
        connect(reset, &QPushButton::clicked, this, [this] {
            play(kBellSound);
            resetTimer();
        });
        connect(background, &QPushButton::clicked, this, [this] { cycleBackground(); });

        // Unlike the Electron build, preferences survive a restart.
        QSettings s;
        theme = s.value("theme", 0).toInt();
        savedSeconds = s.value("savedSeconds", 60).toInt();
        activeSeconds = savedSeconds;
        applyTheme();
        if (!preset.isEmpty()) {
            const int seconds = parseDurationToSeconds(preset);
            if (seconds > 0) {
                savedSeconds = seconds;
                activeSeconds = seconds;
            }
        }
        input->setText(QString::number(savedSeconds));
        updateConversions();
        input->setFocus();

        // `omatimer 25m --start` starts counting without a keypress.
        if (autostart)
            QTimer::singleShot(0, this, [this] { startOrStop(); });
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            play(kBellSound);
            startOrStop();
            return;
        case Qt::Key_R:
            play(kBellSound);
            resetTimer();
            return;
        case Qt::Key_M:
            muted = !muted;
            return;
        case Qt::Key_B:
            cycleBackground();
            return;
        case Qt::Key_Escape:
            close();
            return;
        default:
            QWidget::keyPressEvent(event);
        }
    }

    // Type scales with the window, the way the CSS media queries did.
    void resizeEvent(QResizeEvent *event) override
    {
        const int base = qBound(7, height() / 34, 16);
        QFont f = font();
        f.setPointSize(base);
        for (QWidget *w : {static_cast<QWidget *>(conversions),
                           static_cast<QWidget *>(started),
                           static_cast<QWidget *>(ends),
                           static_cast<QWidget *>(background)})
            w->setFont(f);

        QFont big = input->font();
        big.setPointSize(qBound(18, height() / 8, 72));
        input->setFont(big);

        QFont b = font();
        b.setPointSize(qBound(12, height() / 18, 34));
        playPause->setFont(b);
        this->reset->setFont(b);

        QWidget::resizeEvent(event);
    }

    void closeEvent(QCloseEvent *event) override
    {
        QSettings s;
        s.setValue("theme", theme);
        s.setValue("savedSeconds", savedSeconds);
        QWidget::closeEvent(event);
    }

private:
    void startOrStop()
    {
        running = !running;
        playPause->setText(running ? QString::fromUtf8("⏸")
                                   : QString::fromUtf8("▶"));
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

        deadline = QDateTime::currentDateTime().addSecs(activeSeconds);
        started->setText("Started: " + clockString(QDateTime::currentDateTime()));
        ends->setText("Ends at: " + clockString(deadline));
        input->setText(QString::number(activeSeconds));
        updateConversions();
        ticker->start();
    }

    // Count down against a wall-clock deadline so a slow tick can't drift.
    void tick()
    {
        activeSeconds = static_cast<int>(
            QDateTime::currentDateTime().secsTo(deadline));
        if (activeSeconds < 0)
            activeSeconds = 0;

        input->setText(QString::number(activeSeconds));
        updateConversions();
        ends->setText("Ends at: " + clockString(deadline));

        if (activeSeconds <= 0) {
            ticker->stop();
            running = false;
            playPause->setText(QString::fromUtf8("▶"));
            ends->setText(QString::number(savedSeconds) + "sec: done!");
            input->setText(QString::number(savedSeconds));
            play(kCompleteSound);
        }
    }

    void resetTimer()
    {
        ticker->stop();
        running = false;
        playPause->setText(QString::fromUtf8("▶"));
        activeSeconds = savedSeconds;
        input->setText(QString::number(savedSeconds));
        started->setText("Started: --:--:--");
        ends->setText("Ends at: --:--:--");
        updateConversions();
    }

    void updateConversions()
    {
        conversions->setText(QString("%1min = %2hr")
                                 .arg(activeSeconds / 60.0, 0, 'f', 1)
                                 .arg(activeSeconds / 3600.0, 0, 'f', 2));
    }

    void cycleBackground()
    {
        theme = (theme + 1) % 3;
        applyTheme();
    }

    void applyTheme()
    {
        static const char *icons[] = {"☀", "☾", "○"};
        background->setText(QString::fromUtf8(icons[theme]));
        QString fg, bg;
        switch (theme) {
        case 0: bg = "#2a2a2a"; fg = "#cccccc"; break;  // dark
        case 1: bg = "#ffffff"; fg = "#1a1a1a"; break;  // light
        default: bg = "#000000"; fg = "#ffffff"; break; // high contrast
        }
        setStyleSheet(QString("QWidget { background: %1; color: %2; }"
                              "QLineEdit { background: %1; color: %2; border: none; }"
                              "QPushButton { background: %1; color: %2; border: none; }")
                          .arg(bg, fg));
    }

    void play(const QString &path)
    {
        if (muted || !QFileInfo::exists(path))
            return;
        player->setSource(QUrl::fromLocalFile(path));
        player->play();
    }

    QLabel *conversions, *started, *ends;
    QLineEdit *input;
    QPushButton *playPause, *reset, *background;
    QTimer *ticker;
    QMediaPlayer *player;
    QAudioOutput *audio;
    QDateTime deadline;
    int activeSeconds = 60, savedSeconds = 60, theme = 0;
    bool running = false, muted = false;
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
