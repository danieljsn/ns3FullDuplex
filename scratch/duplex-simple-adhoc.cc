/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 The Boeing Company
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
 */

// 
// This script configures two nodes on an 802.11a physical layer, with
// 802.11a NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to the other node.  The physical layer is configured
// to receive at a fixed RSS (regardless of the distance and transmit
// power); therefore, changing position of the nodes has no effect. 
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc --help"
//
// For instance, for this configuration, the physical layer will
// stop successy receiving packets when rss drops below -97 dBm.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --rss=-97 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-98 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-99 --numPackets=20"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the documentation.
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
// 
// ./waf --run "wifi-simple-adhoc --verbose=1"
//
// When you are done, you will notice two pcap trace files in your directory.
// If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-0-0.pcap -nn -tt
//


#include "ns3/config-store-module.h"
#include "ns3/full-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/tools-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"


#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("FullSimpleAdhoc");

using namespace ns3;

void ReceivePacket (Ptr<Socket> socket)
{
  NS_LOG_UNCOND ("Received one packet!");
}

uint32_t port = 9000;

Vector
GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}
double
GetDistance (Ptr<Node> node1, Ptr<Node> node2)
{
  Vector pos1 = GetPosition (node1);
  Vector pos2 = GetPosition (node2);
  return sqrt (pow (abs (pos1.x - pos2.x), 2) + pow (abs (pos1.y - pos2.y), 2));
}

void
CreateStream (Ptr<Node> src, Ptr<Node> dest, Time start, Time stop, double cbrInterval, uint32_t packetSize)
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

  ////   Install applications: two CBR streams each saturating the channel
    UdpServerHelper server (9);
    server.SetAttribute ("StartTime", TimeValue (start - Seconds(0.1)));
    server.Install (dest);

  UdpClientHelper client1 (ipv4Addrdest, 9);
  client1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  client1.SetAttribute ("StartTime", TimeValue (start));
  client1.SetAttribute ("StopTime", TimeValue (stop));
  client1.SetAttribute ("MaxPackets", UintegerValue (30000)); //(stop - start)/cbrInterval
  client1.SetAttribute ("Interval", TimeValue (Seconds (cbrInterval)));
  client1.Install (src);
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, 
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic, 
                           socket, pktSize,pktCount-1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}


int main (int argc, char *argv[])
{

  LogComponentEnable("FullSimpleAdhoc", LogLevel(LOG_FUNCTION | LOG_ALL ) );
//  LogComponentEnable("FullDcaTxop", LogLevel(LOG_FUNCTION | LOG_ALL| LOG_PREFIX_TIME | LOG_PREFIX_NODE ) );
//  LogComponentEnable("FullAdhocWifiMac", LogLevel(LOG_FUNCTION | LOG_ALL| LOG_PREFIX_TIME | LOG_PREFIX_NODE ) );

//  LogComponentEnable("FullDcfManager", LogLevel(LOG_FUNCTION | LOG_ALL| LOG_PREFIX_TIME | LOG_PREFIX_NODE ) );
//  LogComponentEnable("FlowMonitor", LogLevel(LOG_FUNCTION | LOG_ALL| LOG_PREFIX_TIME | LOG_PREFIX_NODE ) );



  std::string phyMode ("OfdmRate6Mbps");
//  std::string phyMode ("OfdmRate24Mbps");
  double rss = -80;  // -dBm
  uint32_t packetSize = 1000; // bytes
//  uint32_t numPackets = 100;
//  double interval = 1.0; // seconds
  bool verbose = false;

  double startTime = 1;
  double stopTime = 5;
  bool fullDuplex = true;
  bool captureEffect = false;
  bool returnPacket = false;
  bool secondaryPacket = false;
  bool busyTone = false;
  std::string uplinkRate = "6Mbps";
  std::string downlinkRate = "6Mbps";
  std::string mode;

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("startTime", "start time", startTime);
  cmd.AddValue ("stopTime", "stop time", stopTime);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("fullDuplex", "enable full duplex (true) or half duplex (false)", fullDuplex);
  cmd.AddValue ("captureEffect", "enable capture effect in the PHY (true) or not (false)", captureEffect);
  cmd.AddValue ("returnPacket", "enable returnPacket (true) or not (false)", returnPacket);
  cmd.AddValue ("busyTone", "enable sending busytone (true) or not (false)", busyTone);
  cmd.AddValue ("secondaryPacket", "enable forwarding packet (true) or not (false)", secondaryPacket);
  cmd.AddValue("uplinkRate", "uplink data rate", uplinkRate);
  cmd.AddValue("downlinkRate", "downlink data rate", downlinkRate);


  cmd.Parse (argc, argv);
  // Convert to time object
//  Time interPacketInterval = Seconds (interval);

  NS_LOG_INFO ("packet size : " << packetSize);
  NS_LOG_INFO ("phyMode : " << phyMode);


  NodeContainer c;
  c.Create (2);
  NetDeviceContainer devices;

  if (fullDuplex)
    {
      mode = "full";
      NS_LOG_INFO ("full duplex mode");
      if ( captureEffect )
        {
          NS_LOG_INFO ("captureEffect : enabled.");
        }
      else
        {
          NS_LOG_INFO ("captureEffect : disabled.");
        }
      if ( returnPacket )
        {
          NS_LOG_INFO ("returnPacket : enabled.");
        }
      else
        {
          NS_LOG_INFO ("returnPacket : disabled.");
        }
      if ( secondaryPacket )
        {
          NS_LOG_INFO ("secondaryPacket : enabled.");
        }
      else
        {
          NS_LOG_INFO ("secondaryPacket : disabled.");
        }

      // disable fragmentation for frames below 2200 bytes
      Config::SetDefault ("ns3::FullWifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
      // turn off RTS/CTS for frames below 2200 bytes
      Config::SetDefault ("ns3::FullWifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
      // Fix non-unicast data rate to be the same as that of unicast
      Config::SetDefault ("ns3::FullWifiRemoteStationManager::NonUnicastMode",
                          StringValue (phyMode));

      // The below set of helpers will help us to put together the wifi NICs we want
      FullWifiHelper wifi;
      if (verbose)
        {
          wifi.EnableLogComponents ();  // Turn on all Wifi logging
        }
      wifi.SetStandard (FULL_WIFI_PHY_STANDARD_80211a);

      FullYansWifiPhyHelper wifiPhy =  FullYansWifiPhyHelper::Default ();
      // This is one parameter that matters when using FixedRssLossModel
      // set it to zero; otherwise, gain will be added
      wifiPhy.Set ("RxGain", DoubleValue (0) );
      wifiPhy.Set ("EnableCaptureEffect", BooleanValue (captureEffect));
      // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
      wifiPhy.SetPcapDataLinkType (FullYansWifiPhyHelper::DLT_IEEE802_11_RADIO);

      FullYansWifiChannelHelper wifiChannel;
      wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      // The below FixedRssLossModel will cause the rss to be fixed regardless
      // of the distance between the two stations, and the transmit power
      wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
      wifiPhy.SetChannel (wifiChannel.Create ());

      // Add a non-QoS upper mac, and disable rate control
      FullNqosWifiMacHelper wifiMac = FullNqosWifiMacHelper::Default ();
      wifi.SetRemoteStationManager ("ns3::FullConstantRateWifiManager",
                                    "DataMode",StringValue (phyMode),
                                    "ControlMode",StringValue (phyMode));
      // Set it to adhoc mode
      wifiMac.SetType ("ns3::FullAdhocWifiMac");
      wifiMac.Set ("EnableReturnPacket", BooleanValue (returnPacket));
      wifiMac.Set ("EnableBusyTone", BooleanValue (busyTone));
      wifiMac.Set ("EnableForward", BooleanValue (secondaryPacket));

      devices = wifi.Install (wifiPhy, wifiMac, c);

      // Tracing
      wifiPhy.EnablePcap ("full-wifi-simple-adhoc7", devices);
    }
  else
    {
    }

  // Note that with FixedRssLossModel, the positions below are not 
  // used for received signal strength. 
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

//  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
//  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (0), tid);
//  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
//  recvSink->Bind (local);
//  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
//
//  Ptr<Socket> source = Socket::CreateSocket (c.Get (1), tid);
//  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
//  source->SetAllowBroadcast (true);
//  source->Connect (remote);
  DataRate upRate = DataRate (uplinkRate);
  double cbr1 = (packetSize * 8)/ static_cast<double>(upRate.GetBitRate ());
  std::cout << "interval1 :" << cbr1 <<"\n";

  DataRate downRate = DataRate (downlinkRate);
  double cbr2 = (packetSize * 8)/ static_cast<double>(downRate.GetBitRate ());
  std::cout << "interval2 :" << cbr2 <<"\n";
  CreateStream( c.Get(0), c.Get(1), Seconds(startTime), Seconds(stopTime), cbr1, packetSize);
  //CreateStream( c.Get(1), c.Get(0), Seconds(startTime), Seconds(stopTime), cbr2, packetSize);


  // Output what we are doing
//  NS_LOG_UNCOND ("Testing " << numPackets  << " packets sent with receiver rss " << rss );

//  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
//                                  Seconds (1.0), &GenerateTraffic,
//                                  source, packetSize, numPackets, interPacketInterval);


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
      NS_LOG_INFO( "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
//            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
//            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            NS_LOG_INFO( "  Tx Packets:   " << i->second.txPackets);
            NS_LOG_INFO( "  Rx Packets:   " << i->second.rxPackets);
            NS_LOG_INFO( "  Tp: " << tp  << " Mbps");
            NS_LOG_INFO( "  avg delay:   " << delay);
            NS_LOG_INFO( "  Packets Lost: " << i->second.lostPackets);

    }


  std::stringstream ss;
  ss << "runs/" << mode << "_tp" << "_packetSize_" ;//<< (int)packetSize <<"_"<< phyMode ;
  std::string tpFile = ss.str ();
  std::ofstream ftp;
  ftp.open (tpFile.c_str(), std::ofstream::out | std::ofstream::app);
  ftp << avgTp <<"\n";
  ftp.close ();

  std::stringstream ssDelay;

  ssDelay << "runs/" << mode << "_delay" << "_packetSize_" ;// << (int)packetSize <<"_"<< phyMode ;
  std::string delayFile = ssDelay.str ();
  std::ofstream fdelay;
  fdelay.open (delayFile.c_str(), std::ofstream::out | std::ofstream::app);
  fdelay << avgDelay / stats.size () <<"\n";
  fdelay.close ();


//  std::cout << avgTp << "\n" << avgDelay / stats.size () << "\n";
  Simulator::Destroy ();

  return 0;
}

