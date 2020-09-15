
/*----------------------------------------------------------------------

Toy edm::global::EDFilter modules of
edm::*Cache templates and edm::*Producer classes
for testing purposes only.

----------------------------------------------------------------------*/
#include <atomic>
#include <memory>
#include <tuple>
#include <vector>

#include "DataFormats/TestObjects/interface/ToyProducts.h"
#include "FWCore/Framework/interface/CacheHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/LuminosityBlock.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/moduleAbilities.h"
#include "FWCore/Framework/interface/global/EDFilter.h"
#include "FWCore/Framework/interface/ProcessBlock.h"
#include "FWCore/Framework/interface/Run.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/BranchType.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include "FWCore/Utilities/interface/EDGetToken.h"
#include "FWCore/Utilities/interface/InputTag.h"

namespace edmtest {
  namespace global {

    namespace {
      struct Cache {
        Cache() : value(0), run(0), lumi(0), strm(0), work(0) {}
        //Using mutable since we want to update the value.
        mutable std::atomic<unsigned int> value;
        mutable std::atomic<unsigned int> run;
        mutable std::atomic<unsigned int> lumi;
        mutable std::atomic<unsigned int> strm;
        mutable std::atomic<unsigned int> work;
      };

      struct UnsafeCache {
        UnsafeCache() : value(0), run(0), lumi(0), strm(0), work(0) {}
        unsigned int value;
        unsigned int run;
        unsigned int lumi;
        unsigned int strm;
        unsigned int work;
      };

      struct Dummy {};

    }  //end anonymous namespace

    class StreamIntFilter : public edm::global::EDFilter<edm::StreamCache<UnsafeCache>> {
    public:
      explicit StreamIntFilter(edm::ParameterSet const& p) : trans_(p.getParameter<int>("transitions")) {
        produces<unsigned int>();
      }

      const unsigned int trans_;
      mutable std::atomic<unsigned int> m_count{0};

      std::unique_ptr<UnsafeCache> beginStream(edm::StreamID iID) const override {
        ++m_count;
        auto sCache = std::make_unique<UnsafeCache>();
        ++(sCache->strm);
        sCache->value = iID.value();
        return sCache;
      }

      void streamBeginRun(edm::StreamID iID, edm::Run const&, edm::EventSetup const&) const override {
        ++m_count;
        auto sCache = streamCache(iID);
        if (sCache->value != iID.value()) {
          throw cms::Exception("cache value") << (streamCache(iID))->value << " but it was supposed to be " << iID;
        }
        if (sCache->run != 0 || sCache->lumi != 0 || sCache->work != 0 || sCache->strm != 1) {
          throw cms::Exception("out of sequence") << "streamBeginRun out of sequence in Stream " << iID.value();
        }
        ++(sCache->run);
      }

      void streamBeginLuminosityBlock(edm::StreamID iID,
                                      edm::LuminosityBlock const&,
                                      edm::EventSetup const&) const override {
        ++m_count;
        auto sCache = streamCache(iID);
        if (sCache->value != iID.value()) {
          throw cms::Exception("cache value") << (streamCache(iID))->value << " but it was supposed to be " << iID;
        }
        if (sCache->lumi != 0 || sCache->work != 0) {
          throw cms::Exception("out of sequence")
              << "streamBeginLuminosityBlock out of sequence in Stream " << iID.value();
        }
        ++(sCache->lumi);
      }

      bool filter(edm::StreamID iID, edm::Event&, edm::EventSetup const&) const override {
        ++m_count;
        auto sCache = streamCache(iID);
        if (sCache->value != iID.value()) {
          throw cms::Exception("cache value") << (streamCache(iID))->value << " but it was supposed to be " << iID;
        }
        ++(sCache->work);
        if (sCache->lumi == 0 && sCache->run == 0) {
          throw cms::Exception("out of sequence") << "produce out of sequence in Stream " << iID.value();
        }

        return true;
      }

      void streamEndLuminosityBlock(edm::StreamID iID,
                                    edm::LuminosityBlock const&,
                                    edm::EventSetup const&) const override {
        ++m_count;
        auto sCache = streamCache(iID);
        if (sCache->value != iID.value()) {
          throw cms::Exception("cache value") << (streamCache(iID))->value << " but it was supposed to be " << iID;
        }
        --(sCache->lumi);
        sCache->work = 0;
        if (sCache->lumi != 0 || sCache->run == 0) {
          throw cms::Exception("out of sequence")
              << "streamEndLuminosityBlock out of sequence in Stream " << iID.value();
        }
      }

      void streamEndRun(edm::StreamID iID, edm::Run const&, edm::EventSetup const&) const override {
        ++m_count;
        auto sCache = streamCache(iID);
        if (sCache->value != iID.value()) {
          throw cms::Exception("cache value") << (streamCache(iID))->value << " but it was supposed to be " << iID;
        }
        --(sCache->run);
        sCache->work = 0;
        if (sCache->run != 0 || sCache->lumi != 0) {
          throw cms::Exception("out of sequence") << "streamEndRun out of sequence in Stream " << iID.value();
        }
      }

      void endStream(edm::StreamID iID) const override {
        ++m_count;
        auto sCache = streamCache(iID);
        --(sCache->strm);
        if (sCache->value != iID.value()) {
          throw cms::Exception("cache value") << (streamCache(iID))->value << " but it was supposed to be " << iID;
        }
        if (sCache->strm != 0 || sCache->run != 0 || sCache->lumi != 0) {
          throw cms::Exception("out of sequence") << "endStream out of sequence in Stream " << iID.value();
        }
      }

      ~StreamIntFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "StreamIntFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class RunIntFilter : public edm::global::EDFilter<edm::StreamCache<UnsafeCache>, edm::RunCache<Cache>> {
    public:
      explicit RunIntFilter(edm::ParameterSet const& p)
          : trans_(p.getParameter<int>("transitions")), cvalue_(p.getParameter<int>("cachevalue")) {
        produces<unsigned int>();
      }
      const unsigned int trans_;
      const unsigned int cvalue_;
      mutable std::atomic<unsigned int> m_count{0};

      std::shared_ptr<Cache> globalBeginRun(edm::Run const&, edm::EventSetup const&) const override {
        ++m_count;
        auto rCache = std::make_shared<Cache>();
        ++(rCache->run);
        return rCache;
      }

      std::unique_ptr<UnsafeCache> beginStream(edm::StreamID) const override { return std::make_unique<UnsafeCache>(); }

      void streamBeginRun(edm::StreamID iID, edm::Run const& iRun, edm::EventSetup const&) const override {
        auto rCache = runCache(iRun.index());
        if (rCache->run == 0) {
          throw cms::Exception("out of sequence") << "streamBeginRun before globalBeginRun in Stream " << iID.value();
        }
      }

      bool filter(edm::StreamID, edm::Event& iEvent, edm::EventSetup const&) const override {
        ++m_count;
        auto rCache = runCache(iEvent.getRun().index());
        ++(rCache->value);

        return true;
      }

      void streamEndRun(edm::StreamID iID, edm::Run const& iRun, edm::EventSetup const&) const override {
        auto rCache = runCache(iRun.index());
        if (rCache->run == 0) {
          throw cms::Exception("out of sequence") << "streamEndRun after globalEndRun in Stream " << iID.value();
        }
      }

      void globalEndRun(edm::Run const& iRun, edm::EventSetup const&) const override {
        ++m_count;
        auto rCache = runCache(iRun.index());
        if (rCache->value != cvalue_) {
          throw cms::Exception("cache value")
              << "RunIntFilter cache value " << rCache->value << " but it was supposed to be " << cvalue_;
        }
        --(rCache->run);
      }

      ~RunIntFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "RunIntFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class LumiIntFilter
        : public edm::global::EDFilter<edm::StreamCache<UnsafeCache>, edm::LuminosityBlockCache<Cache>> {
    public:
      explicit LumiIntFilter(edm::ParameterSet const& p)
          : trans_(p.getParameter<int>("transitions")), cvalue_(p.getParameter<int>("cachevalue")) {
        produces<unsigned int>();
      }
      const unsigned int trans_;
      const unsigned int cvalue_;
      mutable std::atomic<unsigned int> m_count{0};

      std::shared_ptr<Cache> globalBeginLuminosityBlock(edm::LuminosityBlock const&,
                                                        edm::EventSetup const&) const override {
        ++m_count;
        auto lCache = std::make_shared<Cache>();
        ++(lCache->lumi);
        return lCache;
      }

      std::unique_ptr<UnsafeCache> beginStream(edm::StreamID iID) const override {
        return std::make_unique<UnsafeCache>();
      }

      void streamBeginLuminosityBlock(edm::StreamID iID,
                                      edm::LuminosityBlock const& iLB,
                                      edm::EventSetup const&) const override {
        auto lCache = luminosityBlockCache(iLB.index());
        if (lCache->lumi == 0) {
          throw cms::Exception("out of sequence")
              << "streamBeginLuminosityBlock seen before globalBeginLuminosityBlock in LuminosityBlock"
              << iLB.luminosityBlock();
        }
      }

      bool filter(edm::StreamID, edm::Event& iEvent, edm::EventSetup const&) const override {
        ++m_count;
        ++(luminosityBlockCache(iEvent.getLuminosityBlock().index())->value);
        return true;
      }

      void streamEndLuminosityBlock(edm::StreamID iID,
                                    edm::LuminosityBlock const& iLB,
                                    edm::EventSetup const&) const override {
        auto lCache = luminosityBlockCache(iLB.index());
        if (lCache->lumi == 0) {
          throw cms::Exception("out of sequence")
              << "streamEndLuminosityBlock seen before globalEndLuminosityBlock in LuminosityBlock"
              << iLB.luminosityBlock();
        }
      }

      void globalEndLuminosityBlock(edm::LuminosityBlock const& iLB, edm::EventSetup const&) const override {
        ++m_count;
        auto lCache = luminosityBlockCache(iLB.index());
        --(lCache->lumi);
        if (lCache->lumi != 0) {
          throw cms::Exception("end out of sequence")
              << "globalEndLuminosityBlock seen before globalBeginLuminosityBlock in LuminosityBlock"
              << iLB.luminosityBlock();
        }
        if (lCache->value != cvalue_) {
          throw cms::Exception("cache value")
              << "LumiIntFilter cache value " << lCache->value << " but it was supposed to be " << cvalue_;
        }
      }

      ~LumiIntFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "LumiIntFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class RunSummaryIntFilter
        : public edm::global::EDFilter<edm::StreamCache<Cache>, edm::RunSummaryCache<UnsafeCache>> {
    public:
      explicit RunSummaryIntFilter(edm::ParameterSet const& p)
          : trans_(p.getParameter<int>("transitions")), cvalue_(p.getParameter<int>("cachevalue")) {
        produces<unsigned int>();
      }
      const unsigned int trans_;
      const unsigned int cvalue_;
      mutable std::atomic<unsigned int> m_count{0};

      std::unique_ptr<Cache> beginStream(edm::StreamID) const override {
        ++m_count;
        return std::make_unique<Cache>();
      }

      std::shared_ptr<UnsafeCache> globalBeginRunSummary(edm::Run const&, edm::EventSetup const&) const override {
        ++m_count;
        auto gCache = std::make_shared<UnsafeCache>();
        ++(gCache->run);
        return gCache;
      }

      bool filter(edm::StreamID iID, edm::Event&, edm::EventSetup const&) const override {
        ++m_count;
        ++((streamCache(iID))->value);

        return true;
      }

      void streamEndRunSummary(edm::StreamID iID,
                               edm::Run const&,
                               edm::EventSetup const&,
                               UnsafeCache* gCache) const override {
        ++m_count;
        if (gCache->run == 0) {
          throw cms::Exception("out of sequence")
              << "streamEndRunSummary after globalEndRunSummary in Stream " << iID.value();
        }
        auto sCache = streamCache(iID);
        gCache->value += sCache->value;
        sCache->value = 0;
      }

      void globalEndRunSummary(edm::Run const&, edm::EventSetup const&, UnsafeCache* gCache) const override {
        ++m_count;
        if (gCache->value != cvalue_) {
          throw cms::Exception("cache value")
              << "RunSummaryIntFilter cache value " << gCache->value << " but it was supposed to be " << cvalue_;
        }
        --(gCache->run);
      }

      ~RunSummaryIntFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "RunSummaryIntFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class LumiSummaryIntFilter
        : public edm::global::EDFilter<edm::StreamCache<Cache>, edm::LuminosityBlockSummaryCache<UnsafeCache>> {
    public:
      explicit LumiSummaryIntFilter(edm::ParameterSet const& p)
          : trans_(p.getParameter<int>("transitions")), cvalue_(p.getParameter<int>("cachevalue")) {
        produces<unsigned int>();
      }
      const unsigned int trans_;
      const unsigned int cvalue_;
      mutable std::atomic<unsigned int> m_count{0};

      std::unique_ptr<Cache> beginStream(edm::StreamID) const override {
        ++m_count;
        return std::make_unique<Cache>();
      }

      std::shared_ptr<UnsafeCache> globalBeginLuminosityBlockSummary(edm::LuminosityBlock const& iLB,
                                                                     edm::EventSetup const&) const override {
        ++m_count;
        auto gCache = std::make_shared<UnsafeCache>();
        gCache->lumi = iLB.luminosityBlockAuxiliary().luminosityBlock();
        return gCache;
      }

      bool filter(edm::StreamID iID, edm::Event&, edm::EventSetup const&) const override {
        ++m_count;
        ++((streamCache(iID))->value);
        return true;
      }

      void streamEndLuminosityBlockSummary(edm::StreamID iID,
                                           edm::LuminosityBlock const& iLB,
                                           edm::EventSetup const&,
                                           UnsafeCache* gCache) const override {
        ++m_count;
        if (gCache->lumi != iLB.luminosityBlockAuxiliary().luminosityBlock()) {
          throw cms::Exception("out of sequence")
              << "streamEndLuminosityBlockSummary unexpected lumi number in Stream " << iID.value();
        }
        auto sCache = streamCache(iID);
        gCache->value += sCache->value;
        sCache->value = 0;
      }

      void globalEndLuminosityBlockSummary(edm::LuminosityBlock const& iLB,
                                           edm::EventSetup const&,
                                           UnsafeCache* gCache) const override {
        ++m_count;
        if (gCache->lumi != iLB.luminosityBlockAuxiliary().luminosityBlock()) {
          throw cms::Exception("out of sequence") << "globalEndLuminosityBlockSummary unexpected lumi number";
        }
        if (gCache->value != cvalue_) {
          throw cms::Exception("cache value")
              << "LumiSummaryIntFilter cache value " << gCache->value << " but it was supposed to be " << cvalue_;
        }
      }

      ~LumiSummaryIntFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "LumiSummaryIntFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class ProcessBlockIntFilter : public edm::global::EDFilter<edm::WatchProcessBlock> {
    public:
      explicit ProcessBlockIntFilter(edm::ParameterSet const& p) : trans_(p.getParameter<int>("transitions")) {
        produces<unsigned int>();
        {
          auto tag = p.getParameter<edm::InputTag>("consumesBeginProcessBlock");
          if (not tag.label().empty()) {
            getTokenBegin_ = consumes<unsigned int, edm::InProcess>(tag);
          }
        }
        {
          auto tag = p.getParameter<edm::InputTag>("consumesEndProcessBlock");
          if (not tag.label().empty()) {
            getTokenEnd_ = consumes<unsigned int, edm::InProcess>(tag);
          }
        }
      }

      void beginProcessBlock(edm::ProcessBlock const& processBlock) override {
        if (m_count != 0) {
          throw cms::Exception("transitions")
              << "ProcessBlockIntFilter::begin transitions " << m_count << " but it was supposed to be " << 0;
        }
        ++m_count;
        const unsigned int valueToGet = 31;
        if (not getTokenBegin_.isUninitialized()) {
          if (processBlock.get(getTokenBegin_) != valueToGet) {
            throw cms::Exception("BadValue")
                << "expected " << valueToGet << " but got " << processBlock.get(getTokenBegin_);
          }
        }
      }

      bool filter(edm::StreamID iID, edm::Event&, edm::EventSetup const&) const override {
        if (m_count < 1u) {
          throw cms::Exception("out of sequence") << "produce before beginProcessBlock " << m_count;
        }
        ++m_count;
        return true;
      }

      void endProcessBlock(edm::ProcessBlock const& processBlock) override {
        ++m_count;
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "ProcessBlockIntFilter::end transitions " << m_count << " but it was supposed to be " << trans_;
        }
        {
          const unsigned int valueToGet = 31;
          if (not getTokenBegin_.isUninitialized()) {
            if (processBlock.get(getTokenBegin_) != valueToGet) {
              throw cms::Exception("BadValue")
                  << "expected " << valueToGet << " but got " << processBlock.get(getTokenBegin_);
            }
          }
        }
        {
          const unsigned int valueToGet = 41;
          if (not getTokenEnd_.isUninitialized()) {
            if (processBlock.get(getTokenEnd_) != valueToGet) {
              throw cms::Exception("BadValue")
                  << "expected " << valueToGet << " but got " << processBlock.get(getTokenEnd_);
            }
          }
        }
      }

      ~ProcessBlockIntFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "ProcessBlockIntFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }

    private:
      const unsigned int trans_;
      mutable std::atomic<unsigned int> m_count{0};
      edm::EDGetTokenT<unsigned int> getTokenBegin_;
      edm::EDGetTokenT<unsigned int> getTokenEnd_;
    };

    class TestBeginProcessBlockFilter : public edm::global::EDFilter<edm::BeginProcessBlockProducer> {
    public:
      explicit TestBeginProcessBlockFilter(edm::ParameterSet const& p)
          : trans_(p.getParameter<int>("transitions")),
            token_(produces<unsigned int, edm::Transition::BeginProcessBlock>("begin")) {
        produces<unsigned int>();

        auto tag = p.getParameter<edm::InputTag>("consumesBeginProcessBlock");
        if (not tag.label().empty()) {
          getToken_ = consumes<unsigned int, edm::InProcess>(tag);
        }
      }

      void beginProcessBlockProduce(edm::ProcessBlock& processBlock) override {
        if (m_count != 0) {
          throw cms::Exception("transitions")
              << "TestBeginProcessBlockFilter transitions " << m_count << " but it was supposed to be " << 0;
        }
        ++m_count;
        const unsigned int valueToPutAndGet = 31;
        processBlock.emplace(token_, valueToPutAndGet);

        if (not getToken_.isUninitialized()) {
          if (processBlock.get(getToken_) != valueToPutAndGet) {
            throw cms::Exception("BadValue")
                << "expected " << valueToPutAndGet << " but got " << processBlock.get(getToken_);
          }
        }
      }

      bool filter(edm::StreamID iID, edm::Event&, edm::EventSetup const&) const override {
        if (m_count < 1u) {
          throw cms::Exception("out of sequence") << "produce before beginProcessBlockProduce " << m_count;
        }
        ++m_count;
        return true;
      }

      ~TestBeginProcessBlockFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "TestBeginProcessBlockFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }

    private:
      const unsigned int trans_;
      mutable std::atomic<unsigned int> m_count{0};
      edm::EDPutTokenT<unsigned int> token_;
      edm::EDGetTokenT<unsigned int> getToken_;
    };

    class TestEndProcessBlockFilter : public edm::global::EDFilter<edm::EndProcessBlockProducer> {
    public:
      explicit TestEndProcessBlockFilter(edm::ParameterSet const& p)
          : trans_(p.getParameter<int>("transitions")),
            token_(produces<unsigned int, edm::Transition::EndProcessBlock>("end")) {
        produces<unsigned int>();

        auto tag = p.getParameter<edm::InputTag>("consumesEndProcessBlock");
        if (not tag.label().empty()) {
          getToken_ = consumes<unsigned int, edm::InProcess>(tag);
        }
      }

      bool filter(edm::StreamID iID, edm::Event&, edm::EventSetup const&) const override {
        ++m_count;
        return true;
      }

      void endProcessBlockProduce(edm::ProcessBlock& processBlock) override {
        ++m_count;
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "TestEndProcessBlockFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
        const unsigned int valueToPutAndGet = 41;
        processBlock.emplace(token_, valueToPutAndGet);
        if (not getToken_.isUninitialized()) {
          if (processBlock.get(getToken_) != valueToPutAndGet) {
            throw cms::Exception("BadValue")
                << "expected " << valueToPutAndGet << " but got " << processBlock.get(getToken_);
          }
        }
      }

      ~TestEndProcessBlockFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "~TestEndProcessBlockFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }

    private:
      const unsigned int trans_;
      mutable std::atomic<unsigned int> m_count{0};
      edm::EDPutTokenT<unsigned int> token_;
      edm::EDGetTokenT<unsigned int> getToken_;
    };

    class TestBeginRunFilter : public edm::global::EDFilter<edm::RunCache<Dummy>, edm::BeginRunProducer> {
    public:
      explicit TestBeginRunFilter(edm::ParameterSet const& p) : trans_(p.getParameter<int>("transitions")) {
        produces<unsigned int>();
        produces<unsigned int, edm::Transition::BeginRun>("a");
      }

      const unsigned int trans_;
      mutable std::atomic<unsigned int> m_count{0};
      mutable std::atomic<bool> brp{false};

      std::shared_ptr<Dummy> globalBeginRun(edm::Run const& iRun, edm::EventSetup const&) const override {
        brp = false;
        return std::shared_ptr<Dummy>();
      }

      void globalBeginRunProduce(edm::Run&, edm::EventSetup const&) const override {
        ++m_count;
        brp = true;
      }

      bool filter(edm::StreamID iID, edm::Event&, edm::EventSetup const&) const override {
        if (!brp) {
          throw cms::Exception("out of sequence") << "filter before globalBeginRunProduce in Stream " << iID.value();
        }
        return true;
      }

      void globalEndRun(edm::Run const& iRun, edm::EventSetup const&) const override {}

      ~TestBeginRunFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "TestBeginRunFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class TestEndRunFilter : public edm::global::EDFilter<edm::RunCache<Dummy>, edm::EndRunProducer> {
    public:
      explicit TestEndRunFilter(edm::ParameterSet const& p) : trans_(p.getParameter<int>("transitions")) {
        produces<unsigned int>();
        produces<unsigned int, edm::Transition::EndRun>("a");
      }
      const unsigned int trans_;
      mutable std::atomic<unsigned int> m_count{0};
      mutable std::atomic<bool> p{false};

      std::shared_ptr<Dummy> globalBeginRun(edm::Run const& iRun, edm::EventSetup const&) const override {
        p = false;
        return std::shared_ptr<Dummy>();
      }

      bool filter(edm::StreamID, edm::Event&, edm::EventSetup const&) const override {
        p = true;
        return true;
      }

      void globalEndRunProduce(edm::Run&, edm::EventSetup const&) const override {
        if (!p) {
          throw cms::Exception("out of sequence") << "endRunProduce before produce";
        }
        ++m_count;
      }

      void globalEndRun(edm::Run const& iRun, edm::EventSetup const&) const override {}

      ~TestEndRunFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "TestEndRunFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class TestBeginLumiBlockFilter
        : public edm::global::EDFilter<edm::LuminosityBlockCache<Dummy>, edm::BeginLuminosityBlockProducer> {
    public:
      explicit TestBeginLumiBlockFilter(edm::ParameterSet const& p) : trans_(p.getParameter<int>("transitions")) {
        produces<unsigned int>();
        produces<unsigned int, edm::Transition::BeginLuminosityBlock>("a");
      }
      const unsigned int trans_;
      mutable std::atomic<unsigned int> m_count{0};
      mutable std::atomic<bool> gblp{false};

      std::shared_ptr<Dummy> globalBeginLuminosityBlock(edm::LuminosityBlock const& iLB,
                                                        edm::EventSetup const&) const override {
        gblp = false;
        return std::shared_ptr<Dummy>();
      }

      void globalBeginLuminosityBlockProduce(edm::LuminosityBlock&, edm::EventSetup const&) const override {
        ++m_count;
        gblp = true;
      }

      bool filter(edm::StreamID iID, edm::Event&, const edm::EventSetup&) const override {
        if (!gblp) {
          throw cms::Exception("out of sequence")
              << "filter before globalBeginLuminosityBlockProduce in Stream " << iID.value();
        }
        return true;
      }

      void globalEndLuminosityBlock(edm::LuminosityBlock const& iLB, edm::EventSetup const&) const override {}

      ~TestBeginLumiBlockFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "TestBeginLumiBlockFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class TestEndLumiBlockFilter
        : public edm::global::EDFilter<edm::LuminosityBlockCache<Dummy>, edm::EndLuminosityBlockProducer> {
    public:
      explicit TestEndLumiBlockFilter(edm::ParameterSet const& p) : trans_(p.getParameter<int>("transitions")) {
        produces<unsigned int>();
        produces<unsigned int, edm::Transition::EndLuminosityBlock>("a");
      }
      const unsigned int trans_;
      mutable std::atomic<unsigned int> m_count{0};
      mutable std::atomic<bool> p{false};

      std::shared_ptr<Dummy> globalBeginLuminosityBlock(edm::LuminosityBlock const& iLB,
                                                        edm::EventSetup const&) const override {
        p = false;
        return std::shared_ptr<Dummy>();
      }

      bool filter(edm::StreamID, edm::Event&, edm::EventSetup const&) const override {
        p = true;
        return true;
      }

      void globalEndLuminosityBlockProduce(edm::LuminosityBlock&, edm::EventSetup const&) const override {
        if (!p) {
          throw cms::Exception("out of sequence") << "endLumiBlockProduce before produce";
        }
        ++m_count;
      }

      void globalEndLuminosityBlock(edm::LuminosityBlock const& iLB, edm::EventSetup const&) const override {}

      ~TestEndLumiBlockFilter() {
        if (m_count != trans_) {
          throw cms::Exception("transitions")
              << "TestEndLumiBlockFilter transitions " << m_count << " but it was supposed to be " << trans_;
        }
      }
    };

    class TestInputProcessBlockCache {
    public:
      long long int value_ = 0;
    };

    class TestInputProcessBlockCache1 {
    public:
      long long int value_ = 0;
    };

    class InputProcessBlockIntFilter
        : public edm::global::EDFilter<
              edm::InputProcessBlockCache<int, TestInputProcessBlockCache, TestInputProcessBlockCache1>> {
    public:
      explicit InputProcessBlockIntFilter(edm::ParameterSet const& pset) {
        expectedTransitions_ = pset.getParameter<int>("transitions");
        expectedByRun_ = pset.getParameter<std::vector<int>>("expectedByRun");
        expectedSum_ = pset.getParameter<int>("expectedSum");
        {
          auto tag = pset.getParameter<edm::InputTag>("consumesBeginProcessBlock");
          if (not tag.label().empty()) {
            getTokenBegin_ = consumes<IntProduct, edm::InProcess>(tag);
          }
        }
        {
          auto tag = pset.getParameter<edm::InputTag>("consumesEndProcessBlock");
          if (not tag.label().empty()) {
            getTokenEnd_ = consumes<IntProduct, edm::InProcess>(tag);
          }
        }
        {
          auto tag = pset.getParameter<edm::InputTag>("consumesBeginProcessBlockM");
          if (not tag.label().empty()) {
            getTokenBeginM_ = consumes<IntProduct, edm::InProcess>(tag);
          }
        }
        {
          auto tag = pset.getParameter<edm::InputTag>("consumesEndProcessBlockM");
          if (not tag.label().empty()) {
            getTokenEndM_ = consumes<IntProduct, edm::InProcess>(tag);
          }
        }
        registerProcessBlockCacheFiller<int>(
            getTokenBegin_, [this](edm::ProcessBlock const& processBlock, std::shared_ptr<int> const& previousCache) {
              auto returnValue = std::make_shared<int>(0);
              *returnValue += processBlock.get(getTokenBegin_).value;
              *returnValue += processBlock.get(getTokenEnd_).value;
              ++transitions_;
              return returnValue;
            });
        registerProcessBlockCacheFiller<1>(getTokenBegin_,
                                           [this](edm::ProcessBlock const& processBlock,
                                                  std::shared_ptr<TestInputProcessBlockCache> const& previousCache) {
                                             auto returnValue = std::make_shared<TestInputProcessBlockCache>();
                                             returnValue->value_ += processBlock.get(getTokenBegin_).value;
                                             returnValue->value_ += processBlock.get(getTokenEnd_).value;
                                             ++transitions_;
                                             return returnValue;
                                           });
        registerProcessBlockCacheFiller<TestInputProcessBlockCache1>(
            getTokenBegin_,
            [this](edm::ProcessBlock const& processBlock,
                   std::shared_ptr<TestInputProcessBlockCache1> const& previousCache) {
              auto returnValue = std::make_shared<TestInputProcessBlockCache1>();
              returnValue->value_ += processBlock.get(getTokenBegin_).value;
              returnValue->value_ += processBlock.get(getTokenEnd_).value;
              ++transitions_;
              return returnValue;
            });
      }

      void accessInputProcessBlock(edm::ProcessBlock const& processBlock) override {
        if (processBlock.processName() == "PROD1") {
          sum_ += processBlock.get(getTokenBegin_).value;
          sum_ += processBlock.get(getTokenEnd_).value;
        }
        if (processBlock.processName() == "MERGE") {
          sum_ += processBlock.get(getTokenBeginM_).value;
          sum_ += processBlock.get(getTokenEndM_).value;
        }
        ++transitions_;
      }

      bool filter(edm::StreamID, edm::Event& event, edm::EventSetup const&) const override {
        auto cacheTuple = processBlockCaches(event);
        if (!expectedByRun_.empty()) {
          if (expectedByRun_[event.run()] != *std::get<edm::CacheHandle<int>>(cacheTuple)) {
            throw cms::Exception("UnexpectedValue") << "InputProcessBlockIntFilter::filter cached value was "
                                                    << *std::get<edm::CacheHandle<int>>(cacheTuple)
                                                    << " but it was supposed to be " << expectedByRun_[event.run()];
          }
          if (expectedByRun_[event.run()] != std::get<1>(cacheTuple)->value_) {
            throw cms::Exception("UnexpectedValue")
                << "InputProcessBlockIntFilter::filter second cached value was " << std::get<1>(cacheTuple)->value_
                << " but it was supposed to be " << expectedByRun_[event.run()];
          }
          if (expectedByRun_[event.run()] !=
              std::get<edm::CacheHandle<TestInputProcessBlockCache1>>(cacheTuple)->value_) {
            throw cms::Exception("UnexpectedValue")
                << "InputProcessBlockIntFilter::filter third cached value was "
                << std::get<edm::CacheHandle<TestInputProcessBlockCache1>>(cacheTuple)->value_
                << " but it was supposed to be " << expectedByRun_[event.run()];
          }
        }
        ++transitions_;
        return true;
      }

      void endJob() override {
        if (transitions_ != expectedTransitions_) {
          throw cms::Exception("transitions") << "InputProcessBlockIntFilter transitions " << transitions_
                                              << " but it was supposed to be " << expectedTransitions_;
        }
        if (sum_ != expectedSum_) {
          throw cms::Exception("UnexpectedValue")
              << "InputProcessBlockIntFilter sum " << sum_ << " but it was supposed to be " << expectedSum_;
        }
        if (cacheSize() > 0u) {
          throw cms::Exception("UnexpectedValue")
              << "InputProcessBlockIntFilter cache size not zero at endJob " << cacheSize();
        }
      }

    private:
      edm::EDGetTokenT<IntProduct> getTokenBegin_;
      edm::EDGetTokenT<IntProduct> getTokenEnd_;
      edm::EDGetTokenT<IntProduct> getTokenBeginM_;
      edm::EDGetTokenT<IntProduct> getTokenEndM_;
      CMS_THREAD_SAFE mutable std::atomic<unsigned int> transitions_{0};
      int sum_{0};
      unsigned int expectedTransitions_{0};
      std::vector<int> expectedByRun_;
      int expectedSum_{0};
    };

  }  // namespace global
}  // namespace edmtest

DEFINE_FWK_MODULE(edmtest::global::StreamIntFilter);
DEFINE_FWK_MODULE(edmtest::global::RunIntFilter);
DEFINE_FWK_MODULE(edmtest::global::LumiIntFilter);
DEFINE_FWK_MODULE(edmtest::global::RunSummaryIntFilter);
DEFINE_FWK_MODULE(edmtest::global::LumiSummaryIntFilter);
DEFINE_FWK_MODULE(edmtest::global::ProcessBlockIntFilter);
DEFINE_FWK_MODULE(edmtest::global::TestBeginProcessBlockFilter);
DEFINE_FWK_MODULE(edmtest::global::TestEndProcessBlockFilter);
DEFINE_FWK_MODULE(edmtest::global::TestBeginRunFilter);
DEFINE_FWK_MODULE(edmtest::global::TestBeginLumiBlockFilter);
DEFINE_FWK_MODULE(edmtest::global::TestEndRunFilter);
DEFINE_FWK_MODULE(edmtest::global::TestEndLumiBlockFilter);
DEFINE_FWK_MODULE(edmtest::global::InputProcessBlockIntFilter);
