# impcool_sol

 * A thread pool (for running infinite tasks) that utilizes the concept of immutability to make implementation simpler. 
 * The task buffer is copied upon thread creation and no shared data exists there.
 * Infinite tasks are not popped and removed from the list after one iteration.
 * MIT license.
 
