#Lightweight C++ State Machine

This is a State Machine based off of the state machine code that can be found in the C/C++ Programming Journal (Dr. Dobb's). It is been converted into a shared library optimized by me (Ali Sherief) from the originalto make it thread-safe and fault-free. Invalid State transitions are now blocked by throwing an exception, instead of trapping the code in an infinite loop.

This state machine also supports testing if a transition is allowed, without executing the transition. To allow for this, the transition map macros had to be modified, so this behavior must be enabled manually by defining `#define USE_NEW_TRANSITIONS` before including the `StateMachine.h` header file. Leaving this header undefined will revert the transition maps to their original behavior. It is not supported to mix new transition map code and old transition map code, and you will get compiler errors if you attempt to do so.

See `examples/MotorNew.h` and `examples/MotorNew.cpp` for a working example that uses the new transitions.

## Building

The Windows support code has been retained, so you should be able to easily create a Visual C++ project out of it and build it accordingly. On UNIX systems, you can simply run `make` to build the library and examples. Pthreads is required in order to build on UNIX, nearly all operating systems ship it though.

## Documentation

Documentation for the classes as well as the original README and articles can be found in the `doc` folder.

## License

This library is in the public domain, which is exactly how I found it.
