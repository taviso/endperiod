# endperiod

In some cases, a windows program using `timeBeginPeriod()` can mess with system
power usage. This is usually a minor problem, but it can prevent your system
from entering more aggressive sleep states.

You can read about some example `timeBeginPeriod()` problems in this
[article](https://randomascii.wordpress.com/2013/07/08/windows-timer-resolution-megawatts-wasted/).

This utility can force a process to end a long-running period of lowering the
timer interval, which might help.

## Usage

If you think you need this, first, run the `clockres` utility from
[sysinternals](https://learn.microsoft.com/en-us/sysinternals/downloads/clockres)
when your system is idle.

If `clockres` says the current timer interval is `15.625 ms`, this utility is
not helpful for you and you **do not need it**.

```
$ clockres

Clockres v2.1 - Clock resolution display utility
Copyright (C) 2016 Mark Russinovich
Sysinternals

Maximum timer interval: 15.625 ms
Minimum timer interval: 0.500 ms
Current timer interval: 15.625 ms
```

## Build

Just type `make`.

If you don't want to build it, you can download a pre-built binary from
[releases](https://github.com/taviso/endperiod/releases).

### Investigate

If your timer interval is *lower* than 15.625ms, and you're experiencing
unexplained power problems, then this might help.

First, you need to figure out what process is messing with your timer
interval. Download the
[WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk),
and run the `pwrtest` utility it installed.

It should look like this:

```
$ pwrtest.exe /timer
Device string in use: $Console:enablelvl=Msg|Error|Warn|Assert
WTTLogger_CPP_GitEnlistment(IMProdBldA); Version: 2.7.3483.0

Waiting for Timer Events...
Will run for 30 minutes. Press 'q' anytime to quit...

Timestamp     Event Information
-------------------------------------------------------------------------------
21:29:25.131  TimerResolutionRundown
              (current resolution:156250 minimum resolution: 5000 maximum resolution: 156250 kernel count:0 kernel request:0)
21:29:25.131  TimerResolutionRequestRundown
              (resolution:10000 process:\Device\HarddiskVolume5\Program Files (x86)\TxRadio\radio.exe pid:6400)
```

Hey... this `radio.exe` process is setting the timer really low! That *might*
be a problem? Maybe? There's one way to find out...

This utility can force a long-running process to call `timeEndPeriod()` without
killing it. Yeah, that *might* break the process, *or* it might work fine and
fix your battery issue (it did for me).

YMMV, maybe worth a shot.


### Running

Here is how it looks:

```
$ endperiod32.exe -h
endperiod32.exe [-p <pid> | -n <image>] [-t period]
Attempt to end a timerPeriod in a process interfering with power management
    -p pid      : Attach to the specified process id
    -n image    : Attach to first matching process (e.g. notepad.exe)
    -t period   : Period to end (optiona, I can guess if not specified)
Note: You must use the 64bit version if the target process is 64bit
```

Let's use it on the radio process:

```
$ endperiod32.exe -p 6400
No period specified, current period is 1, I'll use that
Success (result=0), run clockres to see if it worked
> clockres

Clockres v2.1 - Clock resolution display utility
Copyright (C) 2016 Mark Russinovich
Sysinternals

Maximum timer interval: 15.625 ms
Minimum timer interval: 0.500 ms
Current timer interval: 15.625 ms
```

That worked, the current timer interval was released!

I also saw this log message in `pwrtest`:

```
21:36:38.317  UpdateTimerResolution
              (resolution:156250)
```

Now see if your problem is solved.

# Notes

This uses `CreateRemoteThread()` to force a program to call `timeEndPeriod()`,
that means you have to use the 64bit version on 64bit processes, and the 32bit
version on 32bit processes.
