#include "EventFilter/SiStripRawToDigi/plugins/ExcludedFEDListProducer.h"
#include "DataFormats/Common/interface/DetSet.h"
#include "DataFormats/FEDRawData/interface/FEDNumbering.h"
#include "DataFormats/SiStripCommon/interface/SiStripConstants.h"
#include "DataFormats/SiStripCommon/interface/SiStripEventSummary.h"
#include "DataFormats/SiStripDigi/interface/SiStripDigi.h"
#include "DataFormats/SiStripDigi/interface/SiStripRawDigi.h"
#include "EventFilter/SiStripRawToDigi/interface/TFHeaderDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ext/algorithm>

namespace sistrip {

  ExcludedFEDListProducer::ExcludedFEDListProducer(const edm::ParameterSet& pset)
      : runNumber_(0), token_(consumes(pset.getParameter<edm::InputTag>("ProductLabel"))), cablingToken_(esConsumes()) {
    produces<DetIdVector>();
  }

  ExcludedFEDListProducer::~ExcludedFEDListProducer() {}

  void ExcludedFEDListProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("ProductLabel", edm::InputTag("rawDataCollector"));
    descriptions.add("siStripExcludedFEDListProducer", desc);
  }

  void ExcludedFEDListProducer::produce(edm::Event& event, const edm::EventSetup& es) {
    if (runNumber_ != event.run()) {
      runNumber_ = event.run();

      auto const& cabling = es.getData(cablingToken_);

      DetIdVector emptyDetIdVector;
      detids_.swap(emptyDetIdVector);
      // Reserve space in bad module list
      detids_.reserve(100);

      edm::Handle<FEDRawDataCollection> buffers;
      event.getByToken(token_, buffers);

      // Retrieve FED ids from cabling map and iterate through
      for (auto ifed = cabling.fedIds().begin(); ifed != cabling.fedIds().end(); ifed++) {
        // ignore trigger FED
        // if ( *ifed == triggerFedId_ ) { continue; }

        // Retrieve FED raw data for given FED
        const FEDRawData& input = buffers->FEDData(static_cast<int>(*ifed));
        // The FEDData contains a vector<unsigned char>. Check the size of this vector:

        if (input.size() == 0) {
          // std::cout << "Input size == 0 for FED number " << static_cast<int>(*ifed) << std::endl;
          // get the cabling connections for this FED
          auto conns = cabling.fedConnections(*ifed);
          // Mark FED modules as bad
          detids_.reserve(detids_.size() + conns.size());
          for (auto iconn = conns.begin(); iconn != conns.end(); ++iconn) {
            if (!iconn->detId() || iconn->detId() == sistrip::invalid32_)
              continue;
            detids_.push_back(iconn->detId());  //@@ Possible multiple entries
          }
        }
      }
    }

    // for( unsigned int it = 0; it < detids->size(); ++it ) {
    //   std::cout << "detId = " << (*detids)[it]() << std::endl;
    // }

    event.put(std::make_unique<DetIdVector>(detids_));
  }
}  // namespace sistrip
