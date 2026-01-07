/* Option A - Descriptor Table
A Global Descriptor Table (hereinafter referred to as the GDT, not to be confused with the GDT from x86), that will never go out of scope, and is thread-safe.
Each Shared Pointer has a Descriptor ID, the Descriptor ID to a Descriptor is mapped in the GDT.

Descriptor shall contain the underlying_pointer and atomic int for weak and strong references.

To obtain/retain a shared pointer:
Atomic Start:
- Obtain Descriptor from GDT
- Increment Strong/Weak Counter
Atomic End:
- Fail to retain if Descriptor not Found
*/

/* Option B
Constraint: assumes careful and proper usage
Must not try to retain or release an already freed Shared Pointer,
as that would mean the Descriptor would be freed and therefore Undefined Behavior.

It would be good practice to retain on every function that is called,
unless its absolutely expected that an ancestor function will never try to release the Pointer before the function that needs it is done using it.

For thread-safe ownership change, the Calling Thread must increment the strong_count on the Callee Thread's behalf,
it is then the Callee's responsibility to release (decrement the Shared Pointer).

The reason we must do this is that it is possible that in between the Thread being called/spawned and the Thread actually executing,
that the pointer may be freed.
*/