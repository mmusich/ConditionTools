// -*- C++ -*-
//
// Package:    ConditionTools/SiPixelQualityPlotter
// Class:      SiPixelQualityPlotter
//
/**\class SiPixelQualityPlotter SiPixelQualityPlotter.cc ConditionTools/SiPixelQualityPlotter/plugins/SiPixelQualityPlotter.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Marco Musich
//         Created:  Fri, 05 Jun 2020 12:28:51 GMT
//
//

// system include files
#include "CondFormats/DataRecord/interface/SiPixelFedCablingMapRcd.h"
#include "CondFormats/DataRecord/interface/SiPixelQualityFromDbRcd.h"
#include "CondFormats/SiPixelObjects/interface/CablingPathToDetUnit.h"
#include "Geometry/Records/interface/TrackerTopologyRcd.h"
#include "CondFormats/SiPixelObjects/interface/PixelROC.h"
#include "CondFormats/SiPixelObjects/interface/SiPixelFEDChannelContainer.h"
#include "CondFormats/SiPixelObjects/interface/SiPixelFedCablingMap.h"
#include "CondFormats/SiPixelObjects/interface/SiPixelFedCablingTree.h"
#include "CondFormats/SiPixelObjects/interface/SiPixelQuality.h"
#include "CondCore/SiPixelPlugins/interface/SiPixelPayloadInspectorHelper.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "CondCore/CondDB/interface/Time.h"
#include "DataFormats/TrackerCommon/interface/TrackerTopology.h"
#include "CalibTracker/StandaloneTrackerTopology/interface/StandaloneTrackerTopology.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "DataFormats/Luminosity/interface/LumiInfo.h"
#include <iostream>
#include <fstream>      // std::ifstream
#include <memory>
#include "TH2.h"
#include "TCanvas.h"

//
// class declaration
//

// If the analyzer does not use TFileService, please remove
// the template argument to the base class so the class inherits
// from  edm::one::EDAnalyzer<>
// This will improve performance in multithreaded jobs.

class SiPixelQualityPlotter : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
  explicit SiPixelQualityPlotter(const edm::ParameterSet&);
  ~SiPixelQualityPlotter();

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void beginJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void endJob() override;

  // ----------member data ---------------------------

  edm::InputTag lumiInputTag_;
  edm::EDGetTokenT<LumiInfo> lumiToken_;

  int IOVcount_;
  edm::ESWatcher<SiPixelQualityFromDbRcd> SiPixelQualityWatcher_;

  float totalLumi_;
  float lumiSinceLastReset_;
  cond::Time_t lastIOVtime_;

  // actual histograms to fill

  static const int n_layers = 4;
  std::array<TH2D*, n_layers> h_bpix_occ;
  
  static const int n_rings = 2;
  std::array<TH2D*, n_rings> h_fpix_occ;

  // cached payload
  std::map< uint32_t, std::bitset<16> > cachedPayload_;

};

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//
// constructors and destructor
//
SiPixelQualityPlotter::SiPixelQualityPlotter(const edm::ParameterSet& iConfig)
  : lumiInputTag_(iConfig.getUntrackedParameter<edm::InputTag>("lumiInputTag")), lumiToken_(consumes<LumiInfo>(lumiInputTag_)) {

  // initialize the counters

  totalLumi_=0;
  lumiSinceLastReset_=0;
  lastIOVtime_=0;

  // ---------------------    BOOK HISTOGRAMS

  // BPIX
  int nlad_list[n_layers] = {6, 14, 22, 32};
  int divide_roc = 1;

  for (unsigned int lay = 1; lay <= 4; lay++) {
    int nlad = nlad_list[lay - 1];
    
    std::string name = "occ_Layer_" + std::to_string(lay);
    std::string title = "; Module # ; Ladder #";
    h_bpix_occ[lay - 1] = new TH2D(name.c_str(),
				   title.c_str(),
				   72 * divide_roc,
				   -4.5,
				   4.5,
				   (nlad * 4 + 2) * divide_roc,
				   -nlad - 0.5,
				   nlad + 0.5);
  }
  
  // FPIX  
  for (unsigned int ring = 1; ring <= n_rings; ring++) {
    int n = ring == 1 ? 92 : 140;
    float y = ring == 1 ? 11.5 : 17.5;
    std::string name = "occ_ring_" + std::to_string(ring);
    std::string title = "; Disk # ; Blade/Panel #";

    h_fpix_occ[ring - 1] = new TH2D(name.c_str(), title.c_str(), 56 * divide_roc, -3.5, 3.5, n * divide_roc, -y, y);
  }
}

SiPixelQualityPlotter::~SiPixelQualityPlotter() {
  // do anything here that needs to be done at desctruction time
  // (e.g. close files, deallocate resources etc.)
  //
  // please remove this method altogether if it would be left empty
}

//
// member functions
//

// ------------ method called for each event  ------------
void SiPixelQualityPlotter::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;

  unsigned int RunNumber_ = iEvent.eventAuxiliary().run();
  unsigned int LuminosityBlockNumber_ = iEvent.eventAuxiliary().luminosityBlock();

  cond::UnpackedTime localtime = std::make_pair(RunNumber_,LuminosityBlockNumber_);
  cond::Time_t packedtime = cond::time::pack(localtime);
  
  bool hasQualityIOV = SiPixelQualityWatcher_.check(iSetup);

  edm::ESHandle<TrackerTopology> htopo;
  iSetup.get<TrackerTopologyRcd>().get(htopo);
  const auto m_trackerTopo = *htopo.product();

  // retrieve the luminosity
  const LumiInfo& lumi = iEvent.get(lumiToken_);
  totalLumi_+=lumi.getTotalInstLumi();
  lumiSinceLastReset_+=lumi.getTotalInstLumi();

  //if(packedtime!=lastIOVtime_){}

  if (hasQualityIOV) {

    auto lastIOVtime_ = packedtime;

    std::cout << "New IOV, Run: "<< RunNumber_ << " LS:" << LuminosityBlockNumber_ << std::endl;
    std::cout << "Accumulated Luminosity: " << lumiSinceLastReset_ << " | Total Luminosity: " << totalLumi_ << std::endl;

    for ( const auto [mod, payload] : cachedPayload_ ){
      int subid = DetId(mod).subdetId();
      if (subid == PixelSubdetector::PixelBarrel) {

	auto layer = m_trackerTopo.pxbLayer(DetId(mod));
	auto s_ladder = SiPixelPI::signed_ladder(DetId(mod), m_trackerTopo, true);
	auto s_module = SiPixelPI::signed_module(DetId(mod), m_trackerTopo, true);
	
	bool isFlipped = SiPixelPI::isBPixOuterLadder(DetId(mod), m_trackerTopo, false);
	if ((layer > 1 && s_module < 0))
	  isFlipped = !isFlipped;

	auto ladder = m_trackerTopo.pxbLadder(DetId(mod));
	auto module = m_trackerTopo.pxbModule(DetId(mod));
	//std::cout << "layer:" << layer << " ladder:" << ladder << " module:" << module << " signed ladder: " << s_ladder
        //       << " signed module: " << s_module << std::endl;

	auto bpix_rocsToMask = SiPixelPI::maskedBarrelRocsToBins(layer, s_ladder, s_module, payload, isFlipped);
	for (const auto& bin : bpix_rocsToMask) {
	  double x = h_bpix_occ[layer - 1]->GetXaxis()->GetBinCenter(std::get<0>(bin));
	  double y = h_bpix_occ[layer - 1]->GetYaxis()->GetBinCenter(std::get<1>(bin));
	  h_bpix_occ[layer - 1]->Fill(x,y, lumiSinceLastReset_);
	}
      } else if (subid == PixelSubdetector::PixelEndcap) {
	auto ring = SiPixelPI::ring(DetId(mod), m_trackerTopo, true);
	auto s_blade = SiPixelPI::signed_blade(DetId(mod), m_trackerTopo, true);
	auto s_disk = SiPixelPI::signed_disk(DetId(mod), m_trackerTopo, true);
	auto s_blade_panel = SiPixelPI::signed_blade_panel(DetId(mod), m_trackerTopo, true);
	auto panel = m_trackerTopo.pxfPanel(mod);
	
	//bool isFlipped = (s_disk > 0) ? (std::abs(s_blade)%2==0) : (std::abs(s_blade)%2==1);
	bool isFlipped = (s_disk > 0) ? (panel == 1) : (panel == 2);
	
	//std::cout << "ring:" << ring << " blade: " << s_blade << " panel: " << panel
	//	  << " signed blade/panel: " << s_blade_panel << " disk: " << s_disk << std::endl;

	auto fpix_rocsToMask = SiPixelPI::maskedForwardRocsToBins(ring, s_blade, panel, s_disk, payload, isFlipped);
	for (const auto& bin : fpix_rocsToMask) {
	  double x =  h_fpix_occ[ring - 1]->GetXaxis()->GetBinCenter(std::get<0>(bin));
	  double y =  h_fpix_occ[ring - 1]->GetYaxis()->GetBinCenter(std::get<1>(bin));
	  h_fpix_occ[ring - 1]->Fill(x,y,lumiSinceLastReset_);
	}
      } else {
	throw cms::Exception("LogicError") << "Unknown Pixel SubDet ID "<< std::endl;
      }
    }

    // clear the chached payload from memory
    cachedPayload_.clear();
    
    //Retrieve the strip quality from conditions
    edm::ESHandle<SiPixelQuality> siPixelQuality_;
    iSetup.get<SiPixelQualityFromDbRcd>().get(siPixelQuality_);

    // cache the new payload
    auto theDisabledModules = siPixelQuality_->getBadComponentList();
    for (const auto& mod : theDisabledModules) {
      int coded_badRocs = mod.BadRocs;
      std::bitset<16> bad_rocs(coded_badRocs);
      // cache the payload
      cachedPayload_.insert(std::make_pair(mod.DetID,bad_rocs));
    }
    // reset the luminosity count to zero
    lumiSinceLastReset_=0;
  } // if there has been a new IOV
}

// ------------ method called once each job just before starting event loop  ------------
void SiPixelQualityPlotter::beginJob() {
  // please remove this method if not needed
}

// ------------ method called once each job just after ending the event loop  ------------
void SiPixelQualityPlotter::endJob() {
  // please remove this method if not needed
  
  gStyle->SetOptStat(0);

  //=========================
  TCanvas canvasB("SummaryBarrel", "SummaryBarrel", 1200, 1200);
  canvasB.Divide(2, 2);
  for(unsigned int i=1;i<=4;i++){
    canvasB.cd(i)->SetTopMargin(0.05);
    canvasB.cd(i)->SetBottomMargin(0.11);
    canvasB.cd(i)->SetLeftMargin(0.12);
    canvasB.cd(i)->SetRightMargin(0.14);
    canvasB.cd(i)->Modified();
  }  

  for (unsigned int lay = 1; lay <= 4; lay++) {
    h_bpix_occ[lay - 1]->Scale( (100./totalLumi_) );
    SiPixelPI::dress_occup_plot(canvasB, h_bpix_occ[lay - 1], lay, 0, 1);
    SiPixelPI::makeNicePlotStyle(h_bpix_occ[lay - 1]);
    canvasB.cd(lay)->SetLogz();
  }
  
  canvasB.SaveAs("SummaryBarrel.png");

  //=========================
  TCanvas canvasF("SummaryForward", "SummaryForward", 1200, 600);
  canvasF.Divide(2, 1);
  for(unsigned int i=1;i<=2;i++){
    canvasF.cd(i)->SetTopMargin(0.05);
    canvasF.cd(i)->SetBottomMargin(0.11);
    canvasF.cd(i)->SetLeftMargin(0.12);
    canvasF.cd(i)->SetRightMargin(0.14);
    canvasF.cd(i)->Modified();
  }
  for (unsigned int ring = 1; ring <= n_rings; ring++) {
    h_fpix_occ[ring - 1]->Scale( (100./totalLumi_) );
    SiPixelPI::dress_occup_plot(canvasF, h_fpix_occ[ring - 1], 0, ring, 1);
    SiPixelPI::makeNicePlotStyle(h_fpix_occ[ring - 1]);
    canvasF.cd(ring)->SetLogz();
  }

  canvasF.SaveAs("SummaryForward.png");
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void SiPixelQualityPlotter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);

  //Specify that only 'tracks' is allowed
  //To use, remove the default given above and uncomment below
  //ParameterSetDescription desc;
  //desc.addUntracked<edm::InputTag>("tracks","ctfWithMaterialTracks");
  //descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(SiPixelQualityPlotter);
