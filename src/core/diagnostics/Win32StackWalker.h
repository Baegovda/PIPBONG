#pragma once

#include <QStringList>

#include <vector>

#ifdef _WIN32
struct _CONTEXT;
typedef struct _CONTEXT CONTEXT;
#endif

namespace Win32StackWalker {

#ifdef _WIN32
void initialize();
void shutdown();

/// Walks stack for the given thread context (exception or suspended GUI thread).
void appendStackTrace(QStringList& lines, CONTEXT* context, int maxFrames = 32);

/// Stack of the calling thread (watchdog / reporter thread).
void appendCurrentThreadStackTrace(QStringList& lines, int maxFrames = 32);

/// Suspend the GUI thread, capture its context, walk stack, then resume.
/// Returns false when suspend/context walk failed.
bool appendGuiThreadStackTrace(QStringList& lines, unsigned long mainThreadId, int maxFrames = 32);

/// Captures stacks for every thread in the current process except those in @p skipThreadIds.
/// Returns the number of threads captured.
int appendAllProcessThreadStackTraces(QStringList& lines,
                                      const std::vector<unsigned long>& skipThreadIds,
                                      int maxFramesPerThread = 24,
                                      int maxThreads = 32);
#else
inline void initialize() {}
inline void shutdown() {}
#endif

} // namespace Win32StackWalker
