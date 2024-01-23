## Notes
1. The RingBuf 1.0.4 implementation initalizes inner counters to 0 in constructor,
    which messes up the buffers stored in RTC memory - after deep sleep the ring buffers are always fresh clean.
    As a dirty hack I commented out the counter initialization, perhaps I'm just lucky but seems to work fine.