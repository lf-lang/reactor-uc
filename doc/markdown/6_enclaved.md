\page enclaved Enclaved Execution

An enclave is a reactor that has its own scheduler and event queue and thus executes independently from other
reactors in the program. This is equivalent to doing federated execution, but having all the federates
co-exist within the same process and communicating directly through shared memory.

Enclaves are made by denoting a reactor definition as `enclaved`. This has the effect that all reactors within it
turns into enclaves. This is similar to how federates are created by denoting the main reactor as `federated`. 
An enclaved reactor is thus a reactor where all the child reactors are enclaves. An enclaved reactor should not
have any triggers or reactions, only reactor instantiations.

Consider the following program that uses enclaves.
```
reactor Slow {
  timer t(0, 1 sec)
  reaction(t) {=
    env->wait_for(MSEC(500));
  =}
}

reactor Fast {
  timer t(0, 100 msec)
  reaction(t) {=
    printf("Hello from Fast!\n");
  =} deadline (100 msec) {=
    printf("ERROR: Deadline miss");
  =}
}

main enclaved reactor {
  slow = new Slow()
  fast = new Fast()
}

```

The main reactor is marked as enclaved, meaning that the two child reactors, `slow` and `fast` are enclaves. This allows them
to execute completely independently. If they were not enclaves, but instead executing under the same scheduler, `fast`
would experience deadline misses every time `slow` is triggered and blocks execution for 500 msec.





