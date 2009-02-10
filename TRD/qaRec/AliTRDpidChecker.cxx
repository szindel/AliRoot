#include "TAxis.h"
#include "TROOT.h"
#include "TPDGCode.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TH1F.h"
#include "TH1D.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TLegend.h"

#include <TClonesArray.h>
#include <TObjArray.h>
#include <TList.h>

#include "AliESDEvent.h"
#include "AliESDInputHandler.h"
#include "AliTrackReference.h"

#include "AliAnalysisTask.h"

#include "AliTRDtrackerV1.h"
#include "AliTRDtrackV1.h"
#include "AliTRDcluster.h"
#include "AliTRDReconstructor.h"
#include "AliCDBManager.h"
#include "AliTRDpidUtil.h"

#include "AliTRDpidChecker.h"
#include "AliTRDtrackInfo/AliTRDtrackInfo.h"

// calculate pion efficiency at 90% electron efficiency for 11 momentum bins
// this task should be used with simulated data only

ClassImp(AliTRDpidChecker)

//________________________________________________________________________
AliTRDpidChecker::AliTRDpidChecker() 
  :AliTRDrecoTask("PID", "PID Checker")
  ,fReconstructor(0x0)
  ,fUtil(0x0)
  ,fGraph(0x0)
  ,fEfficiency(0x0)
  ,fMomentumAxis(0x0)
 {
  //
  // Default constructor
  //

  fReconstructor = new AliTRDReconstructor();
  fReconstructor->SetRecoParam(AliTRDrecoParam::GetLowFluxParam());

  fUtil = new AliTRDpidUtil();
  InitFunctorList();
}


//________________________________________________________________________
AliTRDpidChecker::~AliTRDpidChecker() 
{
 if(fGraph){fGraph->Delete(); delete fGraph;}
 if(fReconstructor) delete fReconstructor;
 if(fUtil) delete fUtil;
}


//________________________________________________________________________
void AliTRDpidChecker::CreateOutputObjects()
{
  // Create histograms
  // Called once

  OpenFile(0, "RECREATE");
  fContainer = Histos();
}


//_______________________________________________________
TObjArray * AliTRDpidChecker::Histos(){

  //
  // Create QA histograms
  //
  if(fContainer) return fContainer;

  if(!fMomentumAxis){
    Double_t defaultMomenta[AliTRDCalPID::kNMom];
    for(Int_t imom = 0; imom < AliTRDCalPID::kNMom; imom++)
      defaultMomenta[imom] = AliTRDCalPID::GetMomentum(imom);
    SetMomentumBinning(AliTRDCalPID::kNMom - 1, defaultMomenta);
  }
  Int_t xBins = AliPID::kSPECIES*fMomentumAxis->GetNbins(); 
  fContainer = new TObjArray(); fContainer->Expand(7);

  const Float_t epsilon = 1./(2*(AliTRDpidUtil::kBins-1));     // get nice histos with bin center at 0 and 1

  // histos of the electron probability of all 5 particle species and 11 momenta for the 2-dim LQ method 
  fEfficiency = new TObjArray(3);
  fEfficiency->SetOwner(); fEfficiency->SetName("Efficiency");
  fContainer->AddAt(fEfficiency, kEfficiency);
  
  TH1 *h = 0x0;
  if(!(h = (TH2F*)gROOT->FindObject("PID_LQ"))){
    h = new TH2F("PID_LQ", "", xBins, -0.5, xBins - 0.5,
      AliTRDpidUtil::kBins, 0.-epsilon, 1.+epsilon);
  } else h->Reset();
  fEfficiency->AddAt(h, AliTRDpidUtil::kLQ);

  // histos of the electron probability of all 5 particle species and 11 momenta for the neural network method
  if(!(h = (TH2F*)gROOT->FindObject("PID_NN"))){
    h = new TH2F("PID_NN", "", 
      xBins, -0.5, xBins - 0.5,
      AliTRDpidUtil::kBins, 0.-epsilon, 1.+epsilon);
  } else h->Reset();
  fEfficiency->AddAt(h, AliTRDpidUtil::kNN);

  // histos of the electron probability of all 5 particle species and 11 momenta for the ESD output
  if(!(h = (TH2F*)gROOT->FindObject("PID_ESD"))){
    h = new TH2F("PID_ESD", "", 
      xBins, -0.5, xBins - 0.5,
      AliTRDpidUtil::kBins, 0.-epsilon, 1.+epsilon);
  } else h->Reset();
  fEfficiency->AddAt(h, AliTRDpidUtil::kESD);

  // histos of the dE/dx distribution for all 5 particle species and 11 momenta 
  if(!(h = (TH2F*)gROOT->FindObject("dEdx"))){
    h = new TH2F("dEdx", "", 
      xBins, -0.5, xBins - 0.5,
      200, 0, 10000);
  } else h->Reset();
  fContainer->AddAt(h, kdEdx);

  // histos of the dE/dx slices for all 5 particle species and 11 momenta 
  if(!(h = (TH2F*)gROOT->FindObject("dEdxSlice"))){
    h = new TH2F("dEdxSlice", "", 
      xBins*AliTRDpidUtil::kLQslices, -0.5, xBins*AliTRDpidUtil::kLQslices - 0.5,
      200, 0, 5000);
  } else h->Reset();
  fContainer->AddAt(h, kdEdxSlice);

  // histos of the pulse height distribution for all 5 particle species and 11 momenta 
  if(!(h = (TH2F*)gROOT->FindObject("PH"))){
    h = new TProfile2D("PH", "", 
      xBins, -0.5, xBins - 0.5,
      AliTRDtrackerV1::GetNTimeBins(), -0.5, AliTRDtrackerV1::GetNTimeBins() - 0.5);
  } else h->Reset();
  fContainer->AddAt(h, kPH);

  // histos of the number of clusters distribution for all 5 particle species and 11 momenta 
  if(!(h = (TH2F*)gROOT->FindObject("NClus"))){
    h = new TH2F("NClus", "", 
      xBins, -0.5, xBins - 0.5,
      AliTRDtrackerV1::GetNTimeBins(), -0.5, AliTRDtrackerV1::GetNTimeBins() - 0.5);
  } else h->Reset();
  fContainer->AddAt(h, kNClus);


  // momentum distributions - absolute and in momentum bins
  if(!(h = (TH1F*)gROOT->FindObject("hMom"))){
    h = new TH1F("hMom", "momentum distribution", fMomentumAxis->GetNbins(), fMomentumAxis->GetXmin(), fMomentumAxis->GetXmax());
  } else h->Reset();
  fContainer->AddAt(h, kMomentum);
  
  if(!(h = (TH1F*)gROOT->FindObject("hMomBin"))){
    h = new TH1F("hMomBin", "momentum distribution in momentum bins", fMomentumAxis->GetNbins(), fMomentumAxis->GetXmin(), fMomentumAxis->GetXmax());
  } else h->Reset();
  fContainer->AddAt(h, kMomentumBin);


  return fContainer;
}

//________________________________________________________________________
Bool_t AliTRDpidChecker::CheckTrackQuality(const AliTRDtrackV1* track) 
{
  //
  // Check if the track is ok for PID
  //
  
  if(track->GetNumberOfTracklets() == AliTRDgeometry::kNlayer) return 1;
//   if(!fESD)
//     return 0;

  return 0;
}

//________________________________________________________________________
Int_t AliTRDpidChecker::CalcPDG(AliTRDtrackV1* track) 
{

  track -> SetReconstructor(fReconstructor);

  fReconstructor -> SetOption("nn");
  track -> CookPID();

  if(track -> GetPID(AliPID::kElectron) > track -> GetPID(AliPID::kMuon) + track -> GetPID(AliPID::kPion)  + track -> GetPID(AliPID::kKaon) + track -> GetPID(AliPID::kProton)){
    return kElectron;
  }
  else if(track -> GetPID(kProton) > track -> GetPID(AliPID::kPion)  && track -> GetPID(AliPID::kProton) > track -> GetPID(AliPID::kKaon)  && track -> GetPID(AliPID::kProton) > track -> GetPID(AliPID::kMuon)){
    return kProton;
  }
  else if(track -> GetPID(AliPID::kKaon) > track -> GetPID(AliPID::kMuon)  && track -> GetPID(AliPID::kKaon) > track -> GetPID(AliPID::kPion)){
    return kKPlus;
  }
  else if(track -> GetPID(AliPID::kMuon) > track -> GetPID(AliPID::kPion)){
    return kMuonPlus;
  }
  else{
    return kPiPlus;
  }
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotLQ(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }

  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  if(!(fEfficiency = dynamic_cast<TObjArray *>(fContainer->At(kEfficiency)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }
  TH2F *hPIDLQ = 0x0;
  if(!(hPIDLQ = dynamic_cast<TH2F *>(fEfficiency->At(AliTRDpidUtil::kLQ)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }

  AliTRDtrackV1 cTrack(*fTrack);
  cTrack.SetReconstructor(fReconstructor);

  Int_t pdg = 0;
  Float_t momentum = 0.;

  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else{
    //AliWarning("No MC info available!");
    momentum = cTrack.GetMomentum(0);
    pdg = CalcPDG(&cTrack);
  }
  if(!IsInRange(momentum)) return 0x0;

  fReconstructor -> SetOption("!nn");
  cTrack.CookPID();

  Int_t species = AliTRDpidUtil::Pdg2Pid(pdg);
  hPIDLQ -> Fill(FindBin(species, momentum), cTrack.GetPID(AliPID::kElectron));
  return hPIDLQ;
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotNN(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }
  
  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  if(!(fEfficiency = dynamic_cast<TObjArray *>(fContainer->At(kEfficiency)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }
  TH2F *hPIDNN;
  if(!(hPIDNN = dynamic_cast<TH2F *>(fEfficiency->At(AliTRDpidUtil::kNN)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }

  AliTRDtrackV1 cTrack(*fTrack);
  cTrack.SetReconstructor(fReconstructor);

  Int_t pdg = 0;
  Float_t momentum = 0.;
  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else {
    //AliWarning("No MC info available!");
    momentum = cTrack.GetMomentum(0);
    pdg = CalcPDG(&cTrack);
  }
  if(!IsInRange(momentum)) return 0x0;

  fReconstructor -> SetOption("nn");
  cTrack.CookPID();

  Int_t species = AliTRDpidUtil::Pdg2Pid(pdg);
  hPIDNN -> Fill(FindBin(species, momentum), cTrack.GetPID(AliPID::kElectron));
  return hPIDNN;
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotESD(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(!fESD){
    AliWarning("No ESD info available.");
    return 0x0;
  }

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }
  
  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  if(!(fEfficiency = dynamic_cast<TObjArray *>(fContainer->At(kEfficiency)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }
  TH2F *hPIDESD = 0x0;
  if(!(hPIDESD = dynamic_cast<TH2F *>(fEfficiency->At(AliTRDpidUtil::kESD)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }


  Int_t pdg = 0;
  Float_t momentum = 0.;
  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else {
    //AliWarning("No MC info available!");
    AliTRDtrackV1 cTrack(*fTrack);
    cTrack.SetReconstructor(fReconstructor);
    momentum = cTrack.GetMomentum(0);
    pdg = CalcPDG(&cTrack);
  }
  if(!IsInRange(momentum)) return 0x0;

//   Double32_t pidESD[AliPID::kSPECIES];
  const Double32_t *pidESD = fESD->GetResponseIter();
  Int_t species = AliTRDpidUtil::Pdg2Pid(pdg);
  hPIDESD -> Fill(FindBin(species, momentum), pidESD[0]);
  return hPIDESD;
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotdEdx(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }
  
  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  TH2F *hdEdx;
  if(!(hdEdx = dynamic_cast<TH2F *>(fContainer->At(kdEdx)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }

  AliTRDtrackV1 cTrack(*fTrack);
  cTrack.SetReconstructor(fReconstructor);
  Int_t pdg = 0;
  Float_t momentum = 0.;
  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else {
    //AliWarning("No MC info available!");
    momentum = cTrack.GetMomentum(0);
    pdg = CalcPDG(&cTrack);
  }
  if(!IsInRange(momentum)) return 0x0;
  
  Int_t species = AliTRDpidUtil::Pdg2Pid(pdg);
  Float_t SumdEdx = 0;
  Int_t iBin = FindBin(species, momentum);
  for(Int_t iChamb = 0; iChamb < AliTRDgeometry::kNlayer; iChamb++){
    SumdEdx = 0;
    cTrack.GetTracklet(iChamb) -> CookdEdx(AliTRDpidUtil::kLQslices);
    for(Int_t i = 2; i--;) SumdEdx += cTrack.GetTracklet(iChamb)->GetdEdx()[i];
    hdEdx -> Fill(iBin, SumdEdx);
  }
  return hdEdx;
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotdEdxSlice(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }
  
  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  TH2F *hdEdxSlice;
  if(!(hdEdxSlice = dynamic_cast<TH2F *>(fContainer->At(kdEdxSlice)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }

  AliTRDtrackV1 cTrack(*fTrack);
  cTrack.SetReconstructor(fReconstructor);
  Int_t pdg = 0;
  Float_t momentum = 0.;
  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else {
    //AliWarning("No MC info available!");
    momentum = cTrack.GetMomentum(0);
    pdg = CalcPDG(&cTrack);
  }
  if(!IsInRange(momentum)) return 0x0;

  Int_t iMomBin = fMomentumAxis->FindBin(momentum);
  Int_t species = AliTRDpidUtil::Pdg2Pid(pdg);
  Float_t *fdEdx;
  for(Int_t iChamb = 0; iChamb < AliTRDgeometry::kNlayer; iChamb++){
    cTrack.GetTracklet(iChamb) -> CookdEdx(AliTRDpidUtil::kLQslices);
    fdEdx = cTrack.GetTracklet(iChamb)->GetdEdx();
    for(Int_t iSlice = 0; iSlice < AliTRDpidUtil::kLQslices; iSlice++){
      hdEdxSlice -> Fill(species * fMomentumAxis->GetNbins() * AliTRDpidUtil::kLQslices + iMomBin * AliTRDpidUtil::kLQslices + iSlice, fdEdx[iSlice]);
    }
  }  

  return hdEdxSlice;
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotPH(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }
  
  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  TProfile2D *hPH;
  if(!(hPH = dynamic_cast<TProfile2D *>(fContainer->At(kPH)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }

  Int_t pdg = 0;
  Float_t momentum = 0.;
  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else {
    //AliWarning("No MC info available!");
    AliTRDtrackV1 cTrack(*fTrack);
    cTrack.SetReconstructor(fReconstructor);
    momentum = cTrack.GetMomentum(0);
    pdg = CalcPDG(&cTrack);
  }
  if(!IsInRange(momentum)) return 0x0;;

  AliTRDseedV1 *tracklet = 0x0;
  AliTRDcluster *TRDcluster = 0x0;
  Int_t species = AliTRDpidUtil::Pdg2Pid(pdg);
  Int_t iBin = FindBin(species, momentum);
  for(Int_t iChamb = 0; iChamb < AliTRDgeometry::kNlayer; iChamb++){
    tracklet = fTrack->GetTracklet(iChamb);
    for(Int_t iClus = 0; iClus < AliTRDtrackerV1::GetNTimeBins(); iClus++){
      if(!(TRDcluster = tracklet->GetClusters(iClus))) continue;
      hPH -> Fill(iBin, TRDcluster->GetLocalTimeBin(), tracklet->GetdQdl(iClus));
    }
  }
  return hPH;
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotNClus(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }
  
  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  TH2F *hNClus;
  if(!(hNClus = dynamic_cast<TH2F *>(fContainer->At(kNClus)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }


  Int_t pdg = 0;
  Float_t momentum = 0.;
  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else {
    //AliWarning("No MC info available!");
    AliTRDtrackV1 cTrack(*fTrack);
    cTrack.SetReconstructor(fReconstructor);
    momentum = cTrack.GetMomentum(0);
    pdg = CalcPDG(&cTrack);
  }
  if(!IsInRange(momentum)) return 0x0;

  Int_t species = AliTRDpidUtil::AliTRDpidUtil::Pdg2Pid(pdg);
  Int_t iBin = FindBin(species, momentum);
  for(Int_t iChamb = 0; iChamb < AliTRDgeometry::kNlayer; iChamb++)
    hNClus -> Fill(iBin, fTrack->GetTracklet(iChamb)->GetN());

  return hNClus;
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotMom(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }
  
  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  TH1F *hMom;
  if(!(hMom = dynamic_cast<TH1F *>(fContainer->At(kMomentum)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }


  Int_t pdg = 0;
  Float_t momentum = 0.;
  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else {
    //AliWarning("No MC info available!");
    AliTRDtrackV1 cTrack(*fTrack);
    cTrack.SetReconstructor(fReconstructor);
    momentum = cTrack.GetMomentum(0);
    pdg = CalcPDG(&cTrack);
  }
  if(IsInRange(momentum)) hMom -> Fill(momentum);
  return hMom;
}


//_______________________________________________________
TH1 *AliTRDpidChecker::PlotMomBin(const AliTRDtrackV1 *track)
{
  //
  // Plot the probabilities for electrons using 2-dim LQ
  //

  if(track) fTrack = track;
  if(!fTrack){
    AliWarning("No Track defined.");
    return 0x0;
  }
  
  if(!CheckTrackQuality(fTrack)) return 0x0;
  
  TH1F *hMomBin;
  if(!(hMomBin = dynamic_cast<TH1F *>(fContainer->At(kMomentumBin)))){
    AliWarning("No Histogram defined.");
    return 0x0;
  }


  Int_t pdg = 0;
  Float_t momentum = 0.;

  if(fMC){
    if(fMC->GetTrackRef()) momentum = fMC->GetTrackRef()->P();
    pdg = fMC->GetPDG();
  } else {
    //AliWarning("No MC info available!");
    AliTRDtrackV1 cTrack(*fTrack);
    cTrack.SetReconstructor(fReconstructor);
    momentum = cTrack.GetMomentum(0);
  }
  if(IsInRange(momentum)) hMomBin -> Fill(fMomentumAxis->FindBin(momentum));
  return hMomBin;
}


//________________________________________________________
Bool_t AliTRDpidChecker::GetRefFigure(Int_t ifig)
{
  Bool_t FIRST = kTRUE;
  TLegend *leg = new TLegend(.7, .7, .98, .98);
  leg->SetBorderSize(1);
  TGraphErrors *g = 0x0;
  TAxis *ax = 0x0;
  TH1 *h1 = 0x0, *h=0x0;
  TH2 *h2 = 0x0;
  TList *content = 0x0;
  switch(ifig){
  case kEfficiency:
    content = (TList *)fGraph->FindObject("Efficiencies");
    if(!(g = (TGraphErrors*)content->At(AliTRDpidUtil::kLQ))) break;
    if(!g->GetN()) break;
    leg->SetHeader("PID Method");
    g->Draw("apl");
    ax = g->GetHistogram()->GetXaxis();
    ax->SetTitle("p [GeV/c]");
//    ax->SetRangeUser(.6, 10.5);
    ax->SetMoreLogLabels();
    ax = g->GetHistogram()->GetYaxis();
    ax->SetTitle("#epsilon_{#pi} [%]");
    ax->SetRangeUser(1.e-3, 1.e-1);
    leg->AddEntry(g, "2D LQ", "pl");
    if(! (g = (TGraphErrors*)content->At(AliTRDpidUtil::kNN))) break;
    g->Draw("pl");
    leg->AddEntry(g, "NN", "pl");
    if(! (g = (TGraphErrors*)content->At(AliTRDpidUtil::kESD))) break;
    g->Draw("p");
    leg->AddEntry(g, "ESD", "pl");
    leg->Draw();
    gPad->SetLogy();
    gPad->SetLogx();
    gPad->SetGridy();
    gPad->SetGridx();
    return kTRUE;
  case kdEdx:
    // save 2.0 GeV projection as reference
    FIRST = kTRUE;
    if(!(h2 = (TH2F*)(fContainer->At(kdEdx)))) break;
    leg->SetHeader("Particle Species");
    for(Int_t is = AliPID::kSPECIES-1; is>=0; is--){
      Int_t bin = FindBin(is, 2.);
      h1 = h2->ProjectionY("px", bin, bin);
      if(!h1->GetEntries()) continue;
      h1->Scale(1./h1->Integral());
      h1->SetLineColor(AliTRDCalPID::GetPartColor(is));
      h = (TH1F*)h1->DrawClone(FIRST ? "c" : "samec");
      leg->AddEntry(h, Form("%s", AliTRDCalPID::GetPartName(is)), "l");
      FIRST = kFALSE;
    }
    if(FIRST) break;
    leg->Draw();
    gPad->SetLogy();
    gPad->SetLogx(0);
    gPad->SetGridy();
    gPad->SetGridx();
    return kTRUE;
  case kdEdxSlice:
    break;
  case kPH:
    // save 2.0 GeV projection as reference
    FIRST = kTRUE;
    if(!(h2 = (TH2F*)(fContainer->At(kPH)))) break;;
    leg->SetHeader("Particle Species");
    for(Int_t is=0; is<AliPID::kSPECIES; is++){
      Int_t bin = FindBin(is, 2.);
      h1 = h2->ProjectionY("py", bin, bin);
      if(!h1->GetEntries()) continue;
      h1->SetMarkerStyle(24);
      h1->SetMarkerColor(AliTRDCalPID::GetPartColor(is));
      h1->SetLineColor(AliTRDCalPID::GetPartColor(is));
      if(FIRST){
        h1->GetXaxis()->SetTitle("tb[1/100 ns^{-1}]");
        h1->GetYaxis()->SetTitle("<PH> [a.u.]");
      }
      h = (TH1F*)h1->DrawClone(FIRST ? "c" : "samec");
      leg->AddEntry(h, Form("%s", AliTRDCalPID::GetPartName(is)), "pl");
      FIRST = kFALSE;
    }
    if(FIRST) break;
    leg->Draw();
    gPad->SetLogy(0);
    gPad->SetLogx(0);
    gPad->SetGridy();
    gPad->SetGridx();
    return kTRUE;
  case kNClus:
    // save 2.0 GeV projection as reference
    FIRST = kTRUE;
    if(!(h2 = (TH2F*)(fContainer->At(kNClus)))) break;
    leg->SetHeader("Particle Species");
    for(Int_t is=0; is<AliPID::kSPECIES; is++){
      Int_t bin = FindBin(is, 2.);
      h1 = h2->ProjectionY("py", bin, bin);
      if(!h1->GetEntries()) continue;
      //h1->SetMarkerStyle(24);
      //h1->SetMarkerColor(AliTRDCalPID::GetPartColor(is));
      h1->SetLineColor(AliTRDCalPID::GetPartColor(is));
      if(FIRST) h1->GetXaxis()->SetTitle("N^{cl}/tracklet");
      h = (TH1F*)h1->DrawClone(FIRST ? "c" : "samec");
      leg->AddEntry(h, Form("%s", AliTRDCalPID::GetPartName(is)), "l");
      FIRST = kFALSE;
    }
    if(FIRST) break;
    leg->Draw();
    gPad->SetLogy();
    gPad->SetLogx(0);
    gPad->SetGridy();
    gPad->SetGridx();
    return kTRUE;
  case kMomentum:
  case kMomentumBin:
    break; 
  case kThresh:
    content = (TList *)fGraph->FindObject("Thresholds");
    if(!(g = (TGraphErrors*)content->At(AliTRDpidUtil::kLQ))) break;
    if(!g->GetN()) break;
    leg->SetHeader("PID Method");
    g->Draw("apl");
    ax = g->GetHistogram()->GetXaxis();
    ax->SetTitle("p [GeV/c]");
    //ax->SetRangeUser(.6, 10.5);
    ax->SetMoreLogLabels();
    ax = g->GetHistogram()->GetYaxis();
    ax->SetTitle("threshold");
    ax->SetRangeUser(5.e-2, 1.);
    leg->AddEntry(g, "2D LQ", "pl");
    if(!(g = (TGraphErrors*)content->At(AliTRDpidUtil::kNN))) break;
    g->Draw("pl");
    leg->AddEntry(g, "NN", "pl");
    if(!(g = (TGraphErrors*)content->At(AliTRDpidUtil::kESD))) break;
    g->Draw("p");
    leg->AddEntry(g, "ESD", "pl");
    leg->Draw();
    gPad->SetLogx();
    gPad->SetGridy();
    gPad->SetGridx();
    return kTRUE;
  }
  AliInfo(Form("Reference plot [%d] missing result", ifig));
  return kFALSE;
}

//________________________________________________________________________
Bool_t AliTRDpidChecker::PostProcess()
{
  // Draw result to the screen
  // Called once at the end of the query

  if (!fContainer) {
    Printf("ERROR: list not available");
    return kFALSE;
  }
  if(!(fEfficiency = dynamic_cast<TObjArray *>(fContainer->At(kEfficiency)))){
    AliError("Efficiency container missing.");
    return 0x0;
  }
  if(!fGraph){ 
    fGraph = new TObjArray(6);
    fGraph->SetOwner();
    EvaluatePionEfficiency(fEfficiency, fGraph, 0.9);
  }
  fNRefFigures = 8;
  return kTRUE;
}

//________________________________________________________________________
void AliTRDpidChecker::EvaluatePionEfficiency(TObjArray *histoContainer, TObjArray *results, Float_t electron_efficiency){
  TGraphErrors *g = 0x0;
  fUtil->SetElectronEfficiency(electron_efficiency);

  // efficiency graphs
  TGraphErrors *gPtrEff[3], *gPtrThres[3];
  TObjArray *eff = new TObjArray(3); eff->SetOwner(); eff->SetName("Efficiencies");
  results->AddAt(eff, 0);
  eff->AddAt(gPtrEff[AliTRDpidUtil::kLQ] = new TGraphErrors(), AliTRDpidUtil::kLQ);
  g->SetLineColor(kBlue);
  g->SetMarkerColor(kBlue);
  g->SetMarkerStyle(7);
  eff->AddAt(gPtrEff[AliTRDpidUtil::kNN] = new TGraphErrors(), AliTRDpidUtil::kNN);
  g->SetLineColor(kGreen);
  g->SetMarkerColor(kGreen);
  g->SetMarkerStyle(7);
  eff -> AddAt(gPtrEff[AliTRDpidUtil::kESD] = new TGraphErrors(), AliTRDpidUtil::kESD);
  g->SetLineColor(kRed);
  g->SetMarkerColor(kRed);
  g->SetMarkerStyle(24);

  // Threshold graphs
  TObjArray *thres = new TObjArray(3); thres->SetOwner(); thres->SetName("Thresholds");
  results->AddAt(thres, 1);
  thres->AddAt(gPtrThres[AliTRDpidUtil::kLQ] = new TGraphErrors(), AliTRDpidUtil::kLQ);
  g->SetLineColor(kBlue);
  g->SetMarkerColor(kBlue);
  g->SetMarkerStyle(7);
  thres->AddAt(gPtrThres[AliTRDpidUtil::kNN] = new TGraphErrors(), AliTRDpidUtil::kNN);
  g->SetLineColor(kGreen);
  g->SetMarkerColor(kGreen);
  g->SetMarkerStyle(7);
  thres -> AddAt(gPtrThres[AliTRDpidUtil::kESD] = new TGraphErrors(), AliTRDpidUtil::kESD);
  g->SetLineColor(kRed);
  g->SetMarkerColor(kRed);
  g->SetMarkerStyle(24);
  
  Float_t mom = 0.;
  TH1D *Histo1=0x0, *Histo2=0x0;

  TH2F *hPtr[3];
  hPtr[0] = (TH2F*)histoContainer->At(AliTRDpidUtil::kLQ);
  hPtr[1] = (TH2F*)histoContainer->At(AliTRDpidUtil::kNN);
  hPtr[2] = (TH2F*)histoContainer->At(AliTRDpidUtil::kESD);
  TString MethodName[3] = {"LQ", "NN", "ESD"};
  
  for(Int_t iMom = 0; iMom < fMomentumAxis->GetNbins(); iMom++){
    mom = fMomentumAxis->GetBinCenter(iMom);

    Int_t binEl = fMomentumAxis->GetNbins() * AliPID::kElectron + iMom + 1, 
	  binPi = fMomentumAxis->GetNbins() * AliPID::kPion + iMom + 1;
    for(Int_t iMethod = 0; iMethod < 3; iMethod++){
      // Calculate the Pion Efficiency at 90% electron efficiency for each Method
      Histo1 = hPtr[iMethod] -> ProjectionY(Form("%s_ele", MethodName[iMethod].Data()), binEl, binEl);
      Histo2 = hPtr[iMethod] -> ProjectionY(Form("%s_pio", MethodName[iMethod].Data()), binPi, binPi);

      if(!fUtil->CalculatePionEffi(Histo1, Histo2)) continue;
     
      gPtrEff[iMethod]->SetPoint(iMom, mom, fUtil->GetPionEfficiency());
      gPtrEff[iMethod]->SetPointError(iMom, 0., fUtil->GetError());
      gPtrThres[iMethod]->SetPoint(iMom, mom, fUtil->GetThreshold());
      gPtrThres[iMethod]->SetPointError(iMom, 0., 0.);

      if(fDebugLevel>=2) Printf(Form("Pion Efficiency for 2-dim LQ is : %f +/- %f\n\n", MethodName[iMethod].Data()), fUtil->GetPionEfficiency(), fUtil->GetError());
    }
  }
}

//________________________________________________________________________
void AliTRDpidChecker::Terminate(Option_t *) 
{
  // Draw result to the screen
  // Called once at the end of the query

  fContainer = dynamic_cast<TObjArray*>(GetOutputData(0));
  if (!fContainer) {
    Printf("ERROR: list not available");
    return;
  }
}


