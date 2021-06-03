#include "SchedulerState.h"
#include "FramebufferConsole.h"

#include "ipc/Port.h"
#include "sched/Task.h"
#include "sched/Scheduler.h"
#include "sched/GlobalState.h"
#include "printf.h"

using namespace debug;

namespace platform {
    extern debug::FramebufferConsole *gConsole;
};

static void _outchar(char ch, void *) {
    using namespace platform;
    if(gConsole) gConsole->write(ch);
}

void debug::SchedulerStateEntry(uintptr_t arg) {
    reinterpret_cast<SchedulerState *>(arg)->main();
}

/**
 * Creates the scheduler state thread.
 */
SchedulerState::SchedulerState() {
    this->thread = sched::Thread::kernelThread(sched::Task::kern(), SchedulerStateEntry,
            reinterpret_cast<uintptr_t>(this));
    this->thread->setName("thread state debugger");
    this->thread->setState(sched::Thread::State::Runnable);
    sched::Scheduler::get()->markThreadAsRunnable(this->thread, false);
}

/**
 * Terminates the worker thread.
 */
SchedulerState::~SchedulerState() {
    __atomic_store_n(&this->run, false, __ATOMIC_RELAXED);
}

/**
 * Main loop for the printer
 */
void SchedulerState::main() {
    using namespace sched;
    using namespace platform;

    // print header
    gConsole->write("\033[;HThread State ");

    while(this->run) {
        fctprintf(_outchar, 0, "\033[2;HTime: %16lu\n\n", platform_timer_now());

        // iterate over all tasks
        auto gs = GlobalState::the();

        for(const auto &task : gs->getTasks()) {
            fctprintf(_outchar, 0, "%4lu $%p'h    %20s\n", task->pid, task->handle, task->name);

            // print each thread
            fctprintf(_outchar, 0, " \x5  tid Handle              %20s S lv pr %14s %14s %8s %8s %8s %8s\n",
                    "Name", "CPU Time", "Last Sched", "RQ Push", "RQ Pop", "Q used", "Q total");
            for(const auto &thread : task->threads) {
                const auto &sched = thread->sched;

                const char *state;
                switch(thread->state) {
                    case Thread::State::Paused:
                        state = "P";
                        break;
                    case Thread::State::Runnable:
                        state = "\033[32mR\033[m";
                        break;
                    case Thread::State::Blocked:
                        state = "\033[34mB\033[m";
                        break;
                    case Thread::State::Sleeping:
                        state = "\033[36mS\033[m";
                        break;
                    case Thread::State::NotifyWait:
                        state = "\033[34mN\033[m";
                        break;
                    case Thread::State::Zombie:
                        state = "\033[31mZ\033[m";
                        break;
                }

                fctprintf(_outchar, 0, " \x4 %4lu $%p'h %20s %s %2u %2u %14lu %14lu %8lu %8lu %8lu %8lu\n", 
                        thread->tid, thread->handle, thread->name, state, sched.level, sched.lastLevel,
                        sched.cpuTime, thread->lastSwitchedTo, sched.queuePushed, sched.queuePopped,
                        sched.quantumUsed / 10, sched.quantumTotal / 10);
            }

            // print each port
            if(!task->ports.empty()) {
                fctprintf(_outchar, 0, " \x5 Handle              %5s %14s %14s\n",
                        "Pend", "Total Rx", "Total Tx");
                for(const auto &port : task->ports) {
                    fctprintf(_outchar, 0, " \x4 $%p'h %5lu %14lu %14lu\n", port->getHandle(),
                            port->messagesPending(), port->getTotalReceived(), port->getTotalSent());

                }
            }

            // footer
            fctprintf(_outchar, 0, "\n");
        }

        // yeet
        Thread::sleep(1000 * this->updateInterval);
    }

    // terminate thread
    fctprintf(_outchar, 0, "\033[;H\033[41mThread state exited\033[m");
    Thread::current()->terminate();
}
