# impcool_sol

 * A thread pool (for running infinite tasks) that utilizes the concept of immutability to make implementation simpler. 
 * The task buffer is mutated outside of the class via a helper object, and no shared data exists there. The thread unit merely copies it for use.
 * Infinite tasks are not popped and removed from the list after one iteration.
 * unlicense 
