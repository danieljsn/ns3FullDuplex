/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 IITP RAS
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
 * Authors: Pavel Boyko <boyko@iitp.ru>
 */

/*
 * Classical hidden terminal problem and its RTS/CTS solution.
 *
 * Topology: [node 0] <-- -50 dB --> [node 1] <-- -50 dB --> [node 2]
 * 
 * This example illustrates the use of 
 *  - Wifi in ad-hoc mode
 *  - Matrix propagation loss model
 *  - Use of OnOffApplication to generate CBR stream 
 *  - IP flow monitor
 */
#include "ns3/core-module.h"
#include "ns3/propagation-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/full-module.h"

#include <iostream>
#include <vector>

using namespace ns3;

/// Run single 10 seconds experiment with enabled or disabled RTS/CTS mechanism
void experiment (bool enableCtsRts, uint32_t d1, uint32_t d2)
{
  // 0. Enable or disable CTS/RTS
  UintegerValue ctsThr = (enableCtsRts ? UintegerValue (100) : UintegerValue (2200));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);

  // 1. Create 3 nodes 
  NodeContainer nodes;
  nodes.Create (4);

  // 2. Place nodes somehow, this is required by every wireless simulation
  /*for (size_t i = 0; i < 3; ++i)
    {
      nodes.Get (i)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
    }*/

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (d1, 0.0, 0.0));
  positionAlloc->Add (Vector (d2, 0.0, 0.0));
  positionAlloc->Add (Vector (d2+d1, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (nodes);

  // 3. Create propagation loss matrix
  /*Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel> ();
  lossModel->SetDefaultLoss (200); // set default loss to 200 dB (no link)
  lossModel->SetLoss (nodes.Get (0)->GetObject<MobilityModel>(), nodes.Get (1)->GetObject<MobilityModel>(), 50); // set symmetric loss 0 <-> 1 to 50 dB
  lossModel->SetLoss (nodes.Get (2)->GetObject<MobilityModel>(), nodes.Get (1)->GetObject<MobilityModel>(), 50); // set symmetric loss 2 <-> 1 to 50 dB*/
  Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel> ();
  lossModel->SetLambda (3.0e8/5.0e9);

  // 4. Create & setup wifi channel
  Ptr<FullYansWifiChannel> wifiChannel = CreateObject <FullYansWifiChannel> ();
  wifiChannel->SetPropagationLossModel (lossModel);
  wifiChannel->SetPropagationDelayModel (CreateObject <ConstantSpeedPropagationDelayModel> ());

  // 5. Install wireless devices
  FullWifiHelper wifi;
  wifi.SetStandard (FULL_WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager ("ns3::FullConstantRateWifiManager", 
                                "DataMode",StringValue ("OfdmRate54Mbps"), 
                                "ControlMode",StringValue ("OfdmRate6Mbps"));
  FullYansWifiPhyHelper wifiPhy =  FullYansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel);
  wifiPhy.Set ("TxPowerStart", DoubleValue (15)); //potencia de transmissao de 32mW
  wifiPhy.Set ("TxPowerEnd", DoubleValue (15));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (7));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  FullNqosWifiMacHelper wifiMac = FullNqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::FullAdhocWifiMac"); // use ad-hoc MAC
  wifiMac.Set ("EnableReturnPacket", BooleanValue (false));  //A -> B e B -> A
  wifiMac.Set ("EnableBusyTone", BooleanValue (false)); //A -> B e B -> BusyTone
  wifiMac.Set ("EnableForward", BooleanValue (false)); //A -> B e B -> C
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // uncomment the following to have athstats output
  // AthstatsHelper athstats;
  // athstats.EnableAthstats(enableCtsRts ? "basic-athstats-node" : "rtscts-athstats-node", nodes);

  // uncomment the following to have pcap output
  //wifiPhy.EnablePcap (enableCtsRts ? "full-rtscts-pcap-node2" : "full-basic-pcap-test3", nodes);

  // 6. Install TCP/IP stack & assign IP addresses
  InternetStackHelper internet;
  internet.Install (nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.0.0.0");
  ipv4.Assign (devices);

  // 7. Install applications: two CBR streams each saturating the channel 
  ApplicationContainer cbrApps;
  uint16_t cbrPort = 12345;
  //Primeira aplicacao envia do no 1 para o no 2, segunda aplicacao envia do no 3 para o no 4
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.0.0.2"), cbrPort));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (1000));
  onOffHelper.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // flow 1:  node 0 -> node 1
  onOffHelper.SetAttribute ("DataRate", StringValue ("54000000bps"));
  onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.000000)));
  cbrApps.Add (onOffHelper.Install (nodes.Get (0))); 

  // flow 2:  node 2 -> node 1
  // The slightly different start times and data rates are a workround
  // for Bug 388 and Bug 912
  // http://www.nsnam.org/bugzilla/show_bug.cgi?id=912
  // http://www.nsnam.org/bugzilla/show_bug.cgi?id=388
  OnOffHelper onOffHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address ("10.0.0.4"), cbrPort));
  onOffHelper2.SetAttribute ("PacketSize", UintegerValue (1000));
  onOffHelper2.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  onOffHelper2.SetAttribute ("DataRate", StringValue ("54001100bps"));
  onOffHelper2.SetAttribute ("StartTime", TimeValue (Seconds (1.001)));
  cbrApps.Add (onOffHelper2.Install (nodes.Get (2)));

  // we also use separate UDP applications that will send a single
  // packet before the CBR flows start. 
  // This is a workround for the lack of perfect ARP, see Bug 187
  // http://www.nsnam.org/bugzilla/show_bug.cgi?id=187

  uint16_t  echoPort = 9;
  UdpEchoClientHelper echoClientHelper (Ipv4Address ("10.0.0.2"), echoPort);
  echoClientHelper.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClientHelper.SetAttribute ("PacketSize", UintegerValue (10));
  ApplicationContainer pingApps;

  UdpEchoClientHelper echoClientHelper2 (Ipv4Address ("10.0.0.4"), echoPort);
  echoClientHelper2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientHelper2.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClientHelper2.SetAttribute ("PacketSize", UintegerValue (10));

  // again using different start times to workaround Bug 388 and Bug 912
  echoClientHelper.SetAttribute ("StartTime", TimeValue (Seconds (0.001)));
  pingApps.Add (echoClientHelper.Install (nodes.Get (0))); 
  echoClientHelper2.SetAttribute ("StartTime", TimeValue (Seconds (0.006)));
  pingApps.Add (echoClientHelper.Install (nodes.Get (2)));

  // 8. Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // 9. Run simulation for 60 seconds
  Simulator::Stop (Seconds (60));
  Simulator::Run ();

  // 10. Print per flow statistics
  double vazao = 0;
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      // first 2 FlowIds are for ECHO apps, we don't want to display them
      if (i->first > 2)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          //std::cout << "Flow " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          //std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          //std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          //std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 1.0 / 1024 / 1024  << " Mbps\n"; //60s fim da simulacao - 1s inicio dos fluxos
          vazao = vazao + (i->second.rxBytes * 8.0 / 59.0 / 1024 / 1024);
        }
    }

  std::cout << vazao << std::endl;

  // 11. Cleanup
  Simulator::Destroy ();
}

int main (int argc, char **argv)
{
  //uint32_t d1array[] = {10, 20, 30, 40, 50, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 130, 135, 140, 145, 150, 155, 160, 170, 180, 190, 200};
  //uint32_t d2array[] = {20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 260, 270, 280, 290, 300, 330, 360, 390, 420, 450, 480, 510, 540, 570, 600, 630, 660, 690, 720, 750, 780, 810, 840, 870, 900, 930, 960, 990, 1000, 1050, 1100, 1150, 1200, 1250, 1300, 1350, 1400, 1450, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000};

  uint32_t d1array[] = {120};
  uint32_t d2array[] = {1000, 1010, 1020, 1030, 1040, 1050, 1060, 1070, 1080, 1090, 1100, 1110, 1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200, 1210, 1220, 1230, 1240, 1250, 1260, 1270, 1280, 1290, 1300, 1310, 1320, 1330, 1340, 1350, 1360, 1370, 1380, 1390, 1400, 1410, 1420, 1430, 1440, 1450, 1460, 1470, 1480, 1490, 1500, 1510, 1520, 1530, 1540, 1550, 1560, 1570, 1580, 1590, 1600, 1610, 1620, 1630, 1640, 1650, 1660, 1670, 1680, 1690, 1700, 1710, 1720, 1730, 1740, 1750, 1760, 1770, 1780, 1790, 1800, 1810, 1820, 1830, 1840, 1850, 1860, 1870, 1880, 1890, 1900, 1910, 1920, 1930, 1940, 1950, 1960, 1970, 1980, 1990, 2000};

  std::vector<uint32_t> d1vector (d1array, d1array + sizeof(d1array) / sizeof(uint32_t) );
  std::vector<uint32_t> d2vector (d2array, d2array + sizeof(d2array) / sizeof(uint32_t) );

  for (int d1p = 0; d1p < d1vector.size(); d1p++)
  {
    for (int d2p = 0; d2p < d2vector.size(); d2p++)
    {
      if (d1vector[d1p] <= d2vector[d2p])
      {
        //std::cout << "D1 = " << d1vector[d1p] << ", D2 = " << d2vector[d2p] << std::endl;
        std::cout << d1vector[d1p] << " " << d2vector[d2p] << " ";
        //std::cout << "Hidden station experiment with Full Duplex and Busy Tone:\n" << std::flush;
        experiment (false, d1vector[d1p], d2vector[d2p]);
        //std::cout << "------------------------------------------------\n";
        //std::cout << "Hidden station experiment with RTS/CTS enabled:\n";
        //experiment (true, d1vector[d1p], d2vector[d2p]);
      }
    }
  }


  return 0;
}
