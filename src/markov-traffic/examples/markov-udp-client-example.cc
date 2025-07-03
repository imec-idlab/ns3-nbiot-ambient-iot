/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Henrique Duarte Moura <henrique.duartemoura@imec.be>
 *
 */
 #include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/markov-udp-client.h" // Include your custom header

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MarkovUdpClientExample");

/**
  * This example is a minimal ns-3 simulation that demonstrates
  * a Markov-based UDP traffic source generating packets between
  * two nodes connected via a point-to-point link.
  *
  * One node runs a custom MarkovUdpClient application
  * that switches between "ACTIVE" and "INACTIVE" states
  * based on a two-state Markov chain, dynamically adjusting
  * the inter-packet transmission interval to simulate bursty network traffic.
  *
  * The other node runs a UdpServer to receive and log the packets.
  * The simulation illustrates how probabilistic state transitions can control traffic
  * behavior over time, and includes tracing to observe state shifts during execution.
  *
  * Usage:
  * ./waf --run markov-udp-client-example
 */


static void
StateChangeTracer(int oldVal, int newVal)
{
  std::cout << Simulator::Now().GetSeconds() << "s: State changed from "
            << oldVal << " to " << newVal << std::endl;
}


int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("MarkovUdpClient", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install UDP sink on node 1
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  // Install MarkovUdpClient on node 0
  Ptr<MarkovUdpClient> client = CreateObject<MarkovUdpClient>();
  client->SetRemote(interfaces.GetAddress(1), port);
  client->SetAttribute("PacketSize", UintegerValue(512));
  client->SetAttribute("MaxPackets", UintegerValue(1000));
  client->SetRates(Seconds(0.1), Seconds(0.01)); // INACTIVE and ACTIVE intervals
  client->SetTransitionProbabilities(0.1, 0.2);  // P(INACTIVE→ACTIVE), P(ACTIVE→INACTIVE)
  nodes.Get(0)->AddApplication(client);
  client->SetStartTime(Seconds(1.0));
  client->SetStopTime(Seconds(10.0));

  // Optional: trace state transitions
  client->TraceConnectWithoutContext("State", MakeCallback(&StateChangeTracer));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}