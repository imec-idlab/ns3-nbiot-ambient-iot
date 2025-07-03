# Example of a simple Markov UDP client in NS-3

This is a subclass of UdpClient.
It preserves all the features of UdpClient—like sequence numbers and timestamp headers—while giving you Markov-modulated burstiness.


```C++
Ptr<MarkovUdpClient> client = CreateObject<MarkovUdpClient>();
client->SetRemote(interfaces.GetAddress(1), 9);
client->SetPacketSize(1024);
client->SetMaxPackets(1000);
client->SetRates(Seconds(0.1), Seconds(0.01));
client->SetTransitionProbabilities(0.1, 0.2);
nodes.Get(0)->AddApplication(client);
client->SetStartTime(Seconds(1.0));
client->SetStopTime(Seconds(10.0));  // optional. If not set, the application will run indefinitely (i.e., until the simulation ends).
```


You can trace the states of the Markov process using the `MarkovUdpClient::State` signal.
The following example logs the state transitions to a file named `state-transitions.log`.
You can use this to analyze the behavior of the Markov process over time.

```C++
std::ofstream stateLog("state-transitions.log");
client->TraceConnectWithoutContext("State", MakeCallback([&stateLog](int oldVal, int newVal) {
  stateLog << Simulator::Now().GetSeconds() << "s: " << oldVal << " → " << newVal << std::endl;
}));
```



# Example of a simple MMPP traffic application in NS-3


```c++
Ptr<MmppTrafficApp> app = CreateObject<MmppTrafficApp>();
app->SetRemote(InetSocketAddress(Ipv4Address("10.1.1.2"), 9));
app->SetRates(10.0, 100.0); // packets/sec
app->SetTransitionProbabilities(0.1, 0.2);
node->AddApplication(app);
app->SetStartTime(Seconds(1.0));
app->SetStopTime(Seconds(10.0));
```