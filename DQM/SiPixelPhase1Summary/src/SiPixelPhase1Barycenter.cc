// -*- C++ -*-
//
// Package:    SiPixelPhase1Barycenter
// Class:      SiPixelPhase1Barycenter
//
/**\class 

 Description: Create the Phase 1 pixel barycenter plots

 Implementation:
     <Notes on implementation>
*/
//
// Original Author:  Danilo Meuser
//         Created:  26th May 2021
//
//
#include "DQM/SiPixelPhase1Summary/interface/SiPixelPhase1Barycenter.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "DQM/SiPixelPhase1Summary/interface/SiPixelPhase1BarycenterHelper.h"
#include "CalibTracker/StandaloneTrackerTopology/interface/StandaloneTrackerTopology.h"

#include <string>
#include <iostream>

using namespace std;
using namespace edm;

SiPixelPhase1Barycenter::SiPixelPhase1Barycenter(const edm::ParameterSet& iConfig)
    : DQMEDHarvester(iConfig) {
  LogInfo("PixelDQM") << "SiPixelPhase1Barycenter::SiPixelPhase1Barycenter: Got DQM BackEnd interface" << endl;

}

SiPixelPhase1Barycenter::~SiPixelPhase1Barycenter() {
  // do anything here that needs to be done at desctruction time
  // (e.g. close files, deallocate resources etc.)
  LogInfo("PixelDQM") << "SiPixelPhase1Barycenter::~SiPixelPhase1Barycenter: Destructor" << endl;
}

void SiPixelPhase1Barycenter::beginRun(edm::Run const& run, edm::EventSetup const& eSetup) {}

void SiPixelPhase1Barycenter::dqmEndJob(DQMStore::IBooker& iBooker, DQMStore::IGetter& iGetter) {}

void SiPixelPhase1Barycenter::dqmEndLuminosityBlock(DQMStore::IBooker& iBooker,
                                                 DQMStore::IGetter& iGetter,
                                                 const edm::LuminosityBlock& lumiSeg,
                                                 edm::EventSetup const& c) {
                                                   
  
  bookBarycenterHistograms(iBooker);
  
  edm::ESHandle<Alignments> alignment_Handle;
  c.get<TrackerAlignmentRcd>().get(alignment_Handle);
  
  edm::ESHandle<Alignments> gpr_Handle;
  c.get<GlobalPositionRcd>().get(gpr_Handle);
    
  fillBarycenterHistograms(iBooker, iGetter,alignment_Handle->m_align,gpr_Handle->m_align);

}
//------------------------------------------------------------------
// Used to book the barycenter histograms
//------------------------------------------------------------------
void SiPixelPhase1Barycenter::bookBarycenterHistograms(DQMStore::IBooker& iBooker) {
  iBooker.cd();

  iBooker.setCurrentFolder("PixelPhase1/Barycenter");
  //Book one histogram for each subdetector
  for (std::string subdetector : {"BPIX", "FPIXm", "FPIXp"}) {
    barycenters_[subdetector] =
        iBooker.book1D("barycenters_" + subdetector,
                       "Position of the barycenter for " + subdetector + ";Coordinate;Position [mm]",
                       3,
                       0.5,
                       3.5);
        barycenters_[subdetector]->setBinLabel(1, "x");
        barycenters_[subdetector]->setBinLabel(2, "y");
        barycenters_[subdetector]->setBinLabel(3, "z");
  }

  //Reset the iBooker
  iBooker.setCurrentFolder("PixelPhase1/");
}

//------------------------------------------------------------------
// Fill the Barycenter histograms
//------------------------------------------------------------------
void SiPixelPhase1Barycenter::fillBarycenterHistograms(DQMStore::IBooker& iBooker, DQMStore::IGetter& iGetter, const std::vector<AlignTransform>& input, const std::vector<AlignTransform>& GPR) {
  
  const auto GPR_translation_pixel = GPR[0].translation();
  const std::map<DQMBarycenter::coordinate, float> GPR_pixel = {{DQMBarycenter::t_x, GPR_translation_pixel.x()}, {DQMBarycenter::t_y, GPR_translation_pixel.y()}, {DQMBarycenter::t_z, GPR_translation_pixel.z()}};
  
  DQMBarycenter::TkAlBarycenters barycenters;
  TrackerTopology tTopo = StandaloneTrackerTopology::fromTrackerParametersXMLFile(edm::FileInPath("Geometry/TrackerCommonData/data/PhaseI/trackerParameters.xml").fullPath());
  barycenters.computeBarycenters(input, tTopo, GPR_pixel);
  
  auto Xbarycenters = barycenters.getX();
  auto Ybarycenters = barycenters.getY();
  auto Zbarycenters = barycenters.getZ();
  
  //Fill histogram for each subdetector
  std::vector<std::string> subdetectors = {"BPIX", "FPIXm", "FPIXp"};
  for(std::size_t i = 0; i < subdetectors.size(); ++i) {
    barycenters_[subdetectors[i]]->setBinContent(1,Xbarycenters[i]);
    barycenters_[subdetectors[i]]->setBinContent(2,Ybarycenters[i]);
    barycenters_[subdetectors[i]]->setBinContent(3,Zbarycenters[i]);
  }
}

//define this as a plug-in
DEFINE_FWK_MODULE(SiPixelPhase1Barycenter);
