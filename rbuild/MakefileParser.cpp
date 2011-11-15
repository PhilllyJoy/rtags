#include "MakefileParser.h"
#include <QProcess>
#include <QStack>

class DirectoryTracker
{
public:
    DirectoryTracker();

    void init(const Path& path);
    void track(const QByteArray& line);

    const Path& path() const { return mPaths.top(); }

private:
    void enterDirectory(const QByteArray& dir);
    void leaveDirectory(const QByteArray& dir);

private:
    QStack<Path> mPaths;
};

DirectoryTracker::DirectoryTracker()
{
}

void DirectoryTracker::init(const Path& path)
{
    mPaths.push(path);
}

void DirectoryTracker::track(const QByteArray& line)
{
    // printf("Tracking %s\n", line.constData());
    static QRegExp drx(QLatin1String("make[^:]*: ([^ ]+) directory `([^']+)'"));
    if (drx.indexIn(QString::fromLocal8Bit(line.constData())) != -1) {
        if (drx.cap(1) == QLatin1String("Entering"))
            enterDirectory(drx.cap(2).toLocal8Bit());
        else if (drx.cap(1) == QLatin1String("Leaving"))
            leaveDirectory(drx.cap(2).toLocal8Bit());
        else {
            qFatal("Invalid directory track: %s %s",
                   drx.cap(1).toLatin1().constData(),
                   drx.cap(2).toLatin1().constData());
        }
    }
}

void DirectoryTracker::enterDirectory(const QByteArray& dir)
{
    bool ok;
    Path newPath = Path::resolved(dir, path(), &ok);
    if (ok) {
        mPaths.push(newPath);
        // printf("New directory resolved: %s\n", newPath.constData());
    } else {
        qFatal("Unable to resolve path %s (%s)", dir.constData(), path().constData());
    }
}

void DirectoryTracker::leaveDirectory(const QByteArray& /*dir*/)
{
    // qDebug() << "leaveDirectory" << dir;
    // enter and leave share the same code for now
    mPaths.pop();
    // enterDirectory(dir);
}

MakefileParser::MakefileParser(QObject* parent)
    : QObject(parent), mProc(new QProcess(this)), mTracker(new DirectoryTracker)
{
    connect(mProc, SIGNAL(readyReadStandardOutput()),
            this, SLOT(processMakeOutput()));
    connect(mProc, SIGNAL(finished(int)), this, SIGNAL(done()));
}

MakefileParser::~MakefileParser()
{
    delete mTracker;
}

void MakefileParser::run(const Path& makefile)
{
    Q_ASSERT(mProc->state() == QProcess::NotRunning);

    mTracker->init(makefile.parentDir());
    mProc->start(QLatin1String("make"), QStringList() << QLatin1String("-B")
                 << QLatin1String("-j1") << QLatin1String("-n") << QLatin1String("-w")
                 << QLatin1String("-f") << QString::fromLocal8Bit(makefile)
                 << QLatin1String("-C") << mTracker->path());
}

void MakefileParser::processMakeOutput()
{
    mData += mProc->readAllStandardOutput();
    int nextNewline = mData.indexOf('\n');
    while (nextNewline != -1) {
        processMakeLine(mData.left(nextNewline));
        mData = mData.mid(nextNewline + 1);
        nextNewline = mData.indexOf('\n');
    }
}

void MakefileParser::processMakeLine(const QByteArray& line)
{
    GccArguments args;
    if (args.parse(line, mTracker->path()) && args.isCompile()) {
        Q_ASSERT(!args.input().isEmpty());

        emit fileReady(args);
    } else {
        mTracker->track(line);
    }
}
