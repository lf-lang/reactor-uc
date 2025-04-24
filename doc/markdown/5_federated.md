\page federated Federated Execution

reactor-uc supports federated (distributed) execution with decentralized, peer-to-peer coordination based on the PTIDES
principle. This opens up a vast design space within distributed real-time systems.

To make a program distributed, the main reactor is marked as `federated`. This will turn each of the top-level reactors
into a federate.

```
reactor Src {
  
}
```




