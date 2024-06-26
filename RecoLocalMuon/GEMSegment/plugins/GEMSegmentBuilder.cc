#include "RecoLocalMuon/GEMSegment/plugins/GEMSegmentBuilder.h"
#include "DataFormats/MuonDetId/interface/GEMDetId.h"
#include "DataFormats/GEMRecHit/interface/GEMRecHit.h"
#include "Geometry/GEMGeometry/interface/GEMGeometry.h"
#include "Geometry/GEMGeometry/interface/GEMEtaPartition.h"
#include "RecoLocalMuon/GEMSegment/plugins/GEMSegmentAlgorithmBase.h"
#include "RecoLocalMuon/GEMSegment/plugins/GEMSegmentBuilderPluginFactory.h"

#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

GEMSegmentBuilder::GEMSegmentBuilder(const edm::ParameterSet& ps) : geom_(nullptr) {
  // Segment building selection
  enableGE0 = ps.getParameter<bool>("enableGE0");
  enableGE12 = ps.getParameter<bool>("enableGE12");

  // Algo name
  segAlgoName = ps.getParameter<std::string>("algo_name");
  ge0AlgoName = ps.getParameter<std::string>("ge0_name");

  edm::LogVerbatim("GEMSegmentBuilder") << "GEMSegmentBuilder algorithm : ge0 name : " << ge0AlgoName
                                        << " name: " << segAlgoName;

  // SegAlgo parameter set
  segAlgoPSet = ps.getParameter<edm::ParameterSet>("algo_pset");
  ge0AlgoPSet = ps.getParameter<edm::ParameterSet>("ge0_pset");

  // Ask factory to build these algorithms, giving them the appropriate ParameterSets
  segAlgo = GEMSegmentBuilderPluginFactory::get()->create(segAlgoName, segAlgoPSet);
  ge0Algo = GEMSegmentBuilderPluginFactory::get()->create(ge0AlgoName, ge0AlgoPSet);
}
GEMSegmentBuilder::~GEMSegmentBuilder() {}

void GEMSegmentBuilder::fillDescription(edm::ParameterSetDescription& desc) {
  desc.add<bool>("enableGE0", true);
  desc.add<bool>("enableGE12", false);
  desc.add<std::string>("ge0_name", "GE0SegAlgoRU");
  desc.add<std::string>("algo_name", "GEMSegmentAlgorithm");

  edm::ParameterSetDescription ge0AlgoConfigDesc;
  ge0AlgoConfigDesc.add<bool>("allowWideSegments", true);
  ge0AlgoConfigDesc.add<bool>("doCollisions", true);
  ge0AlgoConfigDesc.add<double>("maxChi2Additional", 100);
  ge0AlgoConfigDesc.add<double>("maxChi2Prune", 50);
  ge0AlgoConfigDesc.add<double>("maxChi2GoodSeg", 50);
  ge0AlgoConfigDesc.add<double>("maxPhiSeeds", 0.001096605744)->setComment("Assuming 384 strips");
  ge0AlgoConfigDesc.add<double>("maxPhiAdditional", 0.001096605744)->setComment("Assuming 384 strips");
  ge0AlgoConfigDesc.add<double>("maxETASeeds", 0.1)->setComment("Assuming 8 eta partitions");
  ge0AlgoConfigDesc.add<double>("maxTOFDiff", 25);
  ge0AlgoConfigDesc.add<bool>("requireCentralBX", true)
      ->setComment("require that a majority of hits come from central BX");
  ge0AlgoConfigDesc.add<unsigned int>("minNumberOfHits", 4);
  ge0AlgoConfigDesc.add<unsigned int>("maxNumberOfHits", 300);
  ge0AlgoConfigDesc.add<unsigned int>("maxNumberOfHitsPerLayer", 100);
  desc.add<edm::ParameterSetDescription>("ge0_pset", ge0AlgoConfigDesc);

  edm::ParameterSetDescription recAlgoConfigDesc;
  recAlgoConfigDesc.addUntracked<bool>("GEMDebug", false);
  recAlgoConfigDesc.add<unsigned int>("minHitsPerSegment", 2);
  recAlgoConfigDesc.add<bool>("preClustering", true)
      ->setComment("False => all hits in chamber are given to the fitter");
  recAlgoConfigDesc.add<double>("dXclusBoxMax", 1)->setComment("Clstr Hit dPhi");
  recAlgoConfigDesc.add<double>("dYclusBoxMax", 5)->setComment("Clstr Hit dEta");
  recAlgoConfigDesc.add<bool>("preClusteringUseChaining", true)
      ->setComment("True ==> use Chaining() , False ==> use Clustering() Fnct");
  recAlgoConfigDesc.add<double>("dPhiChainBoxMax", .02)->setComment("Chain Hit dPhi");
  recAlgoConfigDesc.add<double>("dEtaChainBoxMax", .05)->setComment("Chain Hit dEta");
  recAlgoConfigDesc.add<int>("maxRecHitsInCluster", 4)->setComment("Does 4 make sense here?");
  recAlgoConfigDesc.add<bool>("clusterOnlySameBXRecHits", true)
      ->setComment("only working for (preClustering && preClusteringUseChaining)");
  desc.add<edm::ParameterSetDescription>("algo_pset", recAlgoConfigDesc);
}

void GEMSegmentBuilder::build(const GEMRecHitCollection* recHits, GEMSegmentCollection& oc) {
  edm::LogVerbatim("GEMSegmentBuilder") << "[GEMSegmentBuilder::build] Total number of rechits in this event: "
                                        << recHits->size();

  // Let's define the ensemble of GEM devices having the same region, chambers number (phi)
  // different eta partitions and different layers are allowed

  std::map<uint32_t, std::vector<const GEMRecHit*> > ensembleRH;

  // Loop on the GEM rechit and select the different GEM Ensemble
  for (GEMRecHitCollection::const_iterator it2 = recHits->begin(); it2 != recHits->end(); ++it2) {
    // GEM Ensemble is defined by assigning all the GEMDetIds of the same "superchamber"
    // (i.e. region same, chamber same) to the DetId of the first layer

    // here a reference GEMDetId is created: named "id"
    // - Ring 1 (no other rings available for GEM)
    // - Layer 1 = reference layer (effective layermask)
    // - Roll 0  = reference roll  (effective rollmask)
    // - Station == 1 (GE1/1) or == 2 (GE2/1)
    // this reference id serves to link all GEMEtaPartitions
    // and will also be used to determine the GEMSuperChamber
    // to which the GEMSegment is assigned (done inside GEMSegAlgoXX)
    GEMDetId id(it2->gemId().superChamberId());
    // save current GEMRecHit in vector associated to the reference id
    ensembleRH[id.rawId()].push_back(&(*it2));
  }

  // Loop on the entire map <ref id, vector of GEMRecHits>
  for (auto enIt = ensembleRH.begin(); enIt != ensembleRH.end(); ++enIt) {
    std::vector<const GEMRecHit*> gemRecHits;
    std::map<uint32_t, const GEMEtaPartition*> ens;

    // all detIds have been assigned to the to chamber
    const GEMSuperChamber* chamber = geom_->superChamber(enIt->first);
    for (auto rechit = enIt->second.begin(); rechit != enIt->second.end(); ++rechit) {
      gemRecHits.push_back(*rechit);
      ens[(*rechit)->gemId()] = geom_->etaPartition((*rechit)->gemId());
    }

#ifdef EDM_ML_DEBUG  // have lines below only compiled when in debug mode
    LogTrace("GEMSegmentBuilder")
        << "[GEMSegmentBuilder::build] -----------------------------------------------------------------------------";
    LogTrace("GEMSegmentBuilder") << "[GEMSegmentBuilder::build] found " << gemRecHits.size()
                                  << " rechits in GEM Super Chamber " << chamber->id() << " ::";
    for (auto rh = gemRecHits.begin(); rh != gemRecHits.end(); ++rh) {
      auto gemid = (*rh)->gemId();
      // auto rhr = gemGeom->etaPartition(gemid);
      auto rhLP = (*rh)->localPosition();
      // auto rhGP = rhr->toGlobal(rhLP);
      // no sense to print local y because local y here is in the roll reference frame
      // in the roll reference frame the local y of a rechit is always the middle of the roll, and hence equal to 0.0
      LogTrace("GEMSegmentBuilder") << "[RecHit :: Loc x = " << std::showpos << std::setw(9)
                                    << rhLP.x() /*<<" Loc y = "<<std::showpos<<std::setw(9)<<rhLP.y()*/
                                    << " BX = " << (*rh)->BunchX() << " -- " << gemid.rawId() << " = " << gemid << " ]";
    }
#endif

    GEMSegmentAlgorithmBase::GEMEnsemble ensemble(
        std::pair<const GEMSuperChamber*, std::map<uint32_t, const GEMEtaPartition*> >(chamber, ens));

#ifdef EDM_ML_DEBUG  // have lines below only compiled when in debug mode
    LogTrace("GEMSegmentBuilder") << "[GEMSegmentBuilder::build] run the segment reconstruction algorithm now";
#endif

    // given the superchamber select the appropriate algo... and run it
    std::vector<GEMSegment> segv;
    if (enableGE0 and chamber->id().station() == 0)
      segv = ge0Algo->run(ensemble, gemRecHits);
    else if (enableGE12)
      segv = segAlgo->run(ensemble, gemRecHits);
#ifdef EDM_ML_DEBUG  // have lines below only compiled when in debug mode
    LogTrace("GEMSegmentBuilder") << "[GEMSegmentBuilder::build] found " << segv.size();
#endif

    GEMDetId mid(enIt->first);

#ifdef EDM_ML_DEBUG  // have lines below only compiled when in debug mode
    LogTrace("GEMSegmentBuilder") << "[GEMSegmentBuilder::build] found " << segv.size()
                                  << " segments in GEM Super Chamber " << mid;
    LogTrace("GEMSegmentBuilder")
        << "[GEMSegmentBuilder::build] -----------------------------------------------------------------------------";
#endif

    // Add the segments to master collection
    oc.put(mid, segv.begin(), segv.end());
  }
}

void GEMSegmentBuilder::setGeometry(const GEMGeometry* geom) { geom_ = geom; }
