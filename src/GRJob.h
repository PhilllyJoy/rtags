#ifndef GRJob_h
#define GRJob_h

#include "ThreadPool.h"
#include "AbortInterface.h"
#include "Path.h"
#include "Database.h"
#include "signalslot.h"
#include "ScopedDB.h"

class GRJob : public ThreadPool::Job, public AbortInterface
{
public:
    GRJob(const Path &path);
    virtual void run();
    signalslot::Signal1<const List<Path> &> &finished() { return mFinished; }
private:
    static Path::VisitResult visit(const Path &path, void *userData);
    ScopedDB mDB;
    Path mPath;
    Batch *mBatch;
    List<Path> mDirectories;
    signalslot::Signal1<const List<Path> &> mFinished;
};

#endif