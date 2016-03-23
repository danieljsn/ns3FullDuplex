/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006,2007 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/tools-module.h"
#include "ns3/wifi-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("Main");

using namespace ns3;

class Experiment
{
public:
  Experiment ();
  void Run (const WifiHelper &wifi, const YansWifiPhyHelper &wifiPhy, const NqosWifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel);
  
private:
  Vector GetPosition (Ptr<Node> node);
  double GetDistance (Ptr<Node> node1, Ptr<Node> node2);
  Ptr<Socket> SetupPacketReceive (Ptr<Node> node);
  void CreateStream (Ptr<Node> src, Ptr<Node> dest, Time start, Time stop, double cbrInterval, uint32_t packetSize);

  std::string uplinkRate;
  std::string downlinkRate;
  uint32_t packetSize;
  double startTime;
  double stopTime;
};

Experiment::Experiment ()
{
}

void
Experiment::CreateStream (Ptr<Node> src, Ptr<Node> dest, Time start, Time stop, double cbrInterval, uint32_t packetSize)
{
  Vector srcPos = GetPosition (src);
  Vector destPos = GetPosition (dest);

  Ptr<Ipv4> ipv4src = src->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4dest = dest->GetObject<Ipv4>();

  Ipv4InterfaceAddress iaddrsrc = ipv4src->GetAddress (1,0);
  Ipv4InterfaceAddress iaddrdest = ipv4dest->GetAddress (1,0);

  Ipv4Address ipv4Addrsrc = iaddrsrc.GetLocal ();
  Ipv4Address ipv4Addrdest = iaddrdest.GetLocal ();


    NS_LOG_INFO ("App: Src ip " << ipv4Addrsrc
                               << " pos (" << srcPos.x << "," << srcPos.y << ")\n"
                               << "    Dest ip " << ipv4Addrdest
                               << " pos (" << destPos.x << "," << destPos.y << ")"
                               << " dist " << GetDistance (src, dest));

  //cast to void, to suppress variable set but not
  //used compiler warning in optimized builds
  (void) srcPos;
  (void) destPos;
  (void) ipv4Addrsrc;

  //   Install applications: two CBR streams each saturating the channel
  UdpServerHelper server (9);
  server.SetAttribute ("StartTime", TimeValue (start - Seconds(0.1)));
  server.Install (dest);

  UdpClientHelper client1 (ipv4Addrdest, 9);
  client1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  client1.SetAttribute ("StartTime", TimeValue (start));
  client1.SetAttribute ("StopTime", TimeValue (stop));
  client1.SetAttribute ("MaxPackets", UintegerValue (30000)); //escolher um valor maior que ((stop - start)/cbrInterval)
  client1.SetAttribute ("Interval", TimeValue (Seconds (cbrInterval)));
  client1.Install (src);
}

Vector
Experiment::GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}

double
Experiment::GetDistance (Ptr<Node> node1, Ptr<Node> node2)
{
  Vector pos1 = GetPosition (node1);
  Vector pos2 = GetPosition (node2);
  return sqrt (pow (abs (pos1.x - pos2.x), 2) + pow (abs (pos1.y - pos2.y), 2));
}

void
Experiment::Run (const WifiHelper &wifi, const YansWifiPhyHelper &wifiPhy,
                 const NqosWifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel)
{
  uplinkRate = "54Mbps";
  downlinkRate = "54Mbps";
  packetSize = 1000;
  startTime = 1;
  stopTime = 5;

  NodeContainer c;
  c.Create (2);

//  PacketSocketHelper packetSocket;
//  packetSocket.Install (c);

  YansWifiPhyHelper phy = wifiPhy;
  phy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper mac = wifiMac;
  NetDeviceContainer devices = wifi.Install (phy, mac, c);
  
  phy.EnablePcap ("wifi-adhoc3", devices);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (c);
  
  InternetStackHelper internet;
  internet.Install (c);
  
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  DataRate upRate = DataRate (uplinkRate);
  double cbr1 = (packetSize * 8)/ static_cast<double>(upRate.GetBitRate ());
  
  DataRate downRate = DataRate (downlinkRate);
  double cbr2 = (packetSize * 8)/ static_cast<double>(downRate.GetBitRate ());
  
  CreateStream( c.Get(0), c.Get(1), Seconds(startTime), Seconds(stopTime), cbr1, packetSize);
  //CreateStream( c.Get(1), c.Get(0), Seconds(startTime), Seconds(stopTime), cbr2, packetSize);
  
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  std::map<FlowId, FlowMonitor::FlowStats> stats1 = monitor->GetFlowStats ();

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();

  double avgTp = 0;
  double avgDelay = 0;
  double tp = 0;
  double delay = 0;
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  NS_LOG_INFO("number of flows: " << (int) stats.size ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      tp = i->second.rxBytes * 8.0 / (stopTime-startTime) / 1e6;
      delay = (i->second.delaySum.GetMicroSeconds ())/(i->second.rxPackets);
      avgTp += tp;
      avgDelay += delay;
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
/*      NS_LOG_INFO( "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
      NS_LOG_INFO( "  Tx Packets:   " << i->second.txPackets);
      NS_LOG_INFO( "  Rx Packets:   " << i->second.rxPackets);
      NS_LOG_INFO( "  Tp: " << tp  << " Mbps");
      NS_LOG_INFO( "  avg delay:   " << delay);
      NS_LOG_INFO( "  Packets Lost: " << i->second.lostPackets);*/
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
      std::cout << "  Tx Packets:   " << i->second.txPackets << std::endl;
      std::cout << "  Rx Packets:   " << i->second.rxPackets << std::endl;
      std::cout << "  Tp: " << tp  << " Mbps" << std::endl;
      std::cout << "  avg delay:   " << delay << std::endl;
      std::cout << "  Packets Lost: " << i->second.lostPackets << std::endl;
    }

  std::stringstream ss;
  ss << "runs/" << "half" << "_tp" << "_packetSize_" ;//<< (int)packetSize <<"_"<< phyMode ;
  std::string tpFile = ss.str ();
  std::ofstream ftp;
  ftp.open (tpFile.c_str(), std::ofstream::out | std::ofstream::app);
  ftp << avgTp <<"\n";
  ftp.close ();

  std::stringstream ssDelay;

  ssDelay << "runs/" << "half" << "_delay" << "_packetSize_" ;// << (int)packetSize <<"_"<< phyMode ;
  std::string delayFile = ssDelay.str ();
  std::ofstream fdelay;
  fdelay.open (delayFile.c_str(), std::ofstream::out | std::ofstream::app);
  fdelay << avgDelay / stats.size () <<"\n";
  fdelay.close ();
    
  Simulator::Destroy ();

  return;
}

int main (int argc, char *argv[])
{
  // disable fragmentation and RTS/CTS
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Experiment experiment;
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NS_LOG_DEBUG ("54");
  std::cout << "54mb" << std::endl;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate54Mbps"));
  experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);

  NS_LOG_DEBUG ("48");
  std::cout << "48mb" << std::endl;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate48Mbps"));
  experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  
  NS_LOG_DEBUG ("36");
  std::cout << "36mb" << std::endl;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate36Mbps"));
  experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  
  NS_LOG_DEBUG ("24");
  std::cout << "24mb" << std::endl;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate24Mbps"));
  experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  
  NS_LOG_DEBUG ("18");
  std::cout << "18mb" << std::endl;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate18Mbps"));
  experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  
  NS_LOG_DEBUG ("12");
  std::cout << "12mb" << std::endl;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate12Mbps"));
  experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  
  NS_LOG_DEBUG ("9");
  std::cout << "9mb" << std::endl;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate9Mbps"));
  experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  
  NS_LOG_DEBUG ("6");
  std::cout << "6mb" << std::endl;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate6Mbps"));
  experiment.Run (wifi, wifiPhy, wifiMac, wifiChannel);
  
  return 0;
}
