#include <iostream>

// ROOT Includes
#include "TFile.h"
#include "TTree.h"
#include "TGraph.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TLine.h"
#include "TMultiGraph.h"

// ARA Includes
#include "AraGeomTool.h"
#include "RayTraceCorrelator.h"
#include "UsefulAtriStationEvent.h"
#include "/cvmfs/ara.opensciencegrid.org/trunk/centos7/source/libRootFftwWrapper/include/FFTtools.h"

UsefulAtriStationEvent *usefulAtriEvPtr;

// AraSim includes
#include "Detector.h"
#include "Event.h"
#include "Position.h"
#include "RaySolver.h"
#include "IceModel.h"

// #include "helper.h"
#include "polRecoTools.h"

//Initializing some keyword arguments
bool noCutoff = false;
bool debugMode = false;
bool useMcTruth = false;
bool fineRadii = true;
bool useConsoleRadius = false;
bool usePulserCalculation = false;
bool applyBandpass = false;
int minRadius;
int maxRadius;
int deltaR;
int debugEvent=0;
double snrThreshold=4;
// double angleResolution=1;
double angular_size = 1.;
double peakSeparation=50.0;  //Minimum separation between peaks.  Closest seperation is expected to be ~100 ns.
// char* pedestalLocation = "/fs/project/PAS0654/ARA_DATA/pedestalFiles/";
// double peakSeparation=100.0;  //Minimum separation between peaks.  Closest seperation is expected to be ~100 ns. Testing 80ns as that's the approximate width of a pulse
double minFreq = 200;
double maxFreq = 300;
int order=8;


void getCorrMapPeak( TH2D *theCorrMap_input, double &peakTheta, double &peakPhi, double &peakCorr) {

    int _peakZ, _peakTheta, _peakPhi;
    theCorrMap_input->GetMaximumBin(_peakPhi, _peakTheta, _peakZ);

    double maxCorr = theCorrMap_input->GetMaximum();
    double maxPhi = theCorrMap_input->GetXaxis()->GetBinCenter(_peakPhi);
    double maxTheta = theCorrMap_input->GetYaxis()->GetBinCenter(_peakTheta);

    peakCorr = maxCorr;
    peakPhi = maxPhi;
    peakTheta = maxTheta;
}
 


//TODO: make radii and numScanned a little more dynamic so that it doesn't have to be hardcoded.  Maybe had it scan the rt_tables folder to have ti compile a list of radii that match the station, then defining the radii array using those values?  numScanned would simply just be the length of radii[]. Should also have it scan AFTER the station number is declared.  4/6/2024
// double radiiOld[] = {
//     //   0,  
//     // 150,  
//     // 300,  
//     // 450,  
//     // 600,  
//     // 750,  
//     // 900, 
//     // 1050, 
//     // 1200, 
//     // 1350, 
//     // 1500, 
//     // 1650, 
//     // 1800, 
//     // 1950,
//     // 2100, 
//     // 2250, 
//     2400, 
//     2550, 
//     2700,
//     2850, 
//     3000, 
//     3150, 
//     3300
//     // 3450, 
//     // 3600, 
//     // 3750, 
//     // 3900, 
//     // 4050,
//     // 4200, 
//     // 4350, 
//     // 4500, 
//     // 4650, 
//     // 4800, 
//     // 4950
// };

// double radii[] = {2575};

// const int numScannedOld = sizeof(radiiOld)/sizeof(radiiOld[0]);

int main(int argc, char **argv)
{
    if(argc<6) {
        std::cout << "Usage\n" << argv[0] << " <station> <runnum> <input file> <output_dir> <setup_file> \n";
        std::cout << "e.g.\n" << argv[0] << " 2 http://www.hep.ucl.ac.uk/uhen/ara/monitor/root/run1841/event1841.root setup.txt\n";
        return 0;
    }
    // if (argc>6){
    //     std::string action(argv[6]);
    //     if(action == "debug") {
    //         debugMode = true;
    //     }
    // }
    // if (argc>7){
    //     debugEvent = atoi(argv[7]);
    // }
    if (argc>6){
        int i = 6;
        while(argv[i] != NULL) {
            std::string action(argv[i]);
            if (action == "debug") {
                debugMode = true;
                std::string event(argv[i+1]);
                if (atoi(argv[i+1])) {
                    debugEvent = atoi(argv[i+1]);
                }
            }
            if (action == "radius") {
                useConsoleRadius = true;
                minRadius = atoi(argv[i+1]);
                maxRadius = atoi(argv[i+2]);
                deltaR = atoi(argv[i+3]);
            }
            if (action == "usePulserCalculation") {
                usePulserCalculation = true;
            }  
            if (action == "applyBandpass") {
                applyBandpass = true;
            }    
            
            if (action == "noCutoff") {
                cout << "Bypassing cropping of R pulse." << endl;
                noCutoff = true;
            }                
            i++;
        }
    }    

    int station = atoi(argv[1]);  //This will be made redundant when we include the setupfile. 4/6/2024
    char* runNum = argv[2];
    char* inputFile = argv[3];
    char* outputDir = argv[4];
    char* setupfile = argv[5];

    char outfile_name[400];
    sprintf(outfile_name, "%s/recangle_reco_out_run_%s.root", outputDir, runNum);
    std::cout<<"Output name is "<<outfile_name<<std::endl;
    TFile *fpOut = TFile::Open(outfile_name, "RECREATE");
    if(!fpOut){ std::cerr<<"Cannot open output file "<<fpOut<<std::endl; return -1; }

    TTree *outTree = new TTree("vertexReco", "vertexReco");
    
    //Define radii for vertex Reco based on station being used.
    std::vector<double> radii;
    if (useConsoleRadius) {
        for (int r=minRadius; r<maxRadius; r+=deltaR) {
            radii.push_back(r);
        }        
    }
    else {
        // if (not fineRadii) {
        //     for (int r=0; r<numScannedOld; r++) {
        //         radii.push_back(radiiOld[r]);
        //     }
        // }      
        // else if (station == 1 or station==100) {
        
        if (station == 1 or station==100) {
            for (int r=800; r<1200; r+=10) {
                radii.push_back(r);
            }
        }        
        else if (station == 2) {
            for (int r=100; r<4000; r+=100) {
                radii.push_back(r);
            }
        }    
        else if (station == 3) {
            for (int r=3150; r<3590; r+=10) {
                radii.push_back(r);
            }
        }    
        else if (station == 4) {
            for (int r=3170; r<3600; r+=10) {
                radii.push_back(r);
            }
        }  
        else if (station == 5) {
            for (int r=4130; r<4730; r+=10) {
                radii.push_back(r);
            }
        }
        
    }

    
    int numScanned = radii.size();
    
    if (usePulserCalculation) {
        numScanned = 1;
    }
    
    cout << "numScanned = " << numScanned << endl;

    double peakCorrs_out[numScanned];
    double peakThetas_out[numScanned];
    double peakPhis_out[numScanned];
    int peakPol_out[numScanned];
    int peakSol_out[numScanned];
    double bestTheta_out;
    double bestPhi_out;
    double bestCorr_out;
    double bestR_out;
    int bestSol_out;
    double reco_arrivalThetas_out[16];
    double reco_arrivalPhis_out[16];
    double reco_launchThetas_out[16];
    double reco_launchPhis_out[16];    
    double arrivalTimes_out[16];

    double trueTheta_out;
    double truePhi_out;
    double trueR_out;
    int likelySol_out;
    double true_arrivalThetas_out[16];
    double true_arrivalPhis_out[16];
    double true_launchThetas_out[16];
    double true_launchPhis_out[16];    
    double cutoffTime[16]; 
    double directPeakTimes[16];
    double refPeakTimes[16];
    
    int eventNumber;
    int unixTime;
    int unixTimeUs;
    int timeStamp;    

    double weight_out;
    double weight;
    double snrs_out[16];
    double snrs_fullBand_out[16];
    double v_snr_out;
    double h_snr_out;
    double v_snr_fullBand_out;
    double h_snr_fullBand_out;    
    outTree->Branch("peakCorrs", peakCorrs_out, TString::Format("peakCoors[%d]/D",numScanned));
    outTree->Branch("peakThetas", peakThetas_out, TString::Format("peakThetas[%d]/D",numScanned));
    outTree->Branch("peakPhis", peakPhis_out, TString::Format("peakPhis[%d]/D",numScanned));
    outTree->Branch("peakPols", peakPol_out, TString::Format("peakPol[%d]/I",numScanned));
    outTree->Branch("peakSols", peakSol_out, TString::Format("peakSol[%d]/I",numScanned));
    outTree->Branch("bestTheta", &bestTheta_out, "bestTheta/D");
    outTree->Branch("bestPhi", &bestPhi_out, "bestPhi/D");
    outTree->Branch("bestCorr", &bestCorr_out, "bestCorr/D");
    outTree->Branch("bestR", &bestR_out, "bestR/D");
    outTree->Branch("bestSol", &bestSol_out, "bestSol/I");
    outTree->Branch("reco_arrivalThetas", reco_arrivalThetas_out, "reco_arrivalThetas[16]/D");
    outTree->Branch("reco_arrivalPhis", reco_arrivalPhis_out, "reco_arrivalPhis[16]/D");
    outTree->Branch("reco_launchThetas", reco_launchThetas_out, "reco_launchThetas[16]/D");
    outTree->Branch("reco_launchPhis", reco_launchPhis_out, "reco_launchPhis[16]/D");    
    outTree->Branch("arrivalTimes", arrivalTimes_out, "arrivalTimes[16]/D");

    //These are simulation specific, and should be set to zero or delete if using a real data reconstruction
    outTree->Branch("trueTheta", &trueTheta_out, "trueTheta_out/D");
    outTree->Branch("truePhi", &truePhi_out, "truePhi_out/D");
    outTree->Branch("trueR", &trueR_out, "trueR_out/D");
    outTree->Branch("trueSol", &likelySol_out, "likelySol_out/I");
    outTree->Branch("true_arrivalThetas", true_arrivalThetas_out, "true_arrivalThetas_out[16]/D");
    outTree->Branch("true_arrivalPhis", true_arrivalPhis_out, "true_arrivalPhis_out[16]/D");
    outTree->Branch("true_launchThetas", true_launchThetas_out, "true_launchThetas_out[16]/D");
    outTree->Branch("true_launchPhis", true_launchPhis_out, "true_launchPhis_out[16]/D");    
    
    outTree->Branch("weight", &weight_out, "weight_out/D");
    outTree->Branch("snrs", snrs_out, "snrs_out[16]/D");
    outTree->Branch("snrs_fullBand", snrs_fullBand_out, "snrs_fullBand_out[16]/D");
    outTree->Branch("v_snr", &v_snr_out, "v_snr_out/D");
    outTree->Branch("h_snr", &h_snr_out, "h_snr_out/D");
    outTree->Branch("v_snr_fullBand", &v_snr_fullBand_out, "v_snr_fullBand_out/D");
    outTree->Branch("h_snr_fullBand", &h_snr_fullBand_out, "h_snr_fullBand_out/D");    
    
    //These are to accomodate a real data reconstruction
    outTree->Branch("eventNumber", &eventNumber, "eventNumber/I");
    outTree->Branch("unixTime", &unixTime, "unixTime/I");
    outTree->Branch("unixTimeUs", &unixTimeUs, "unixTimeUs/I");
    outTree->Branch("timeStamp", &timeStamp, "timeStamp/I");
    
    //This should work for any double-peak event, as simulated events can show D and R solution in a large enough trigger window.
    outTree->Branch("cutoffTime", &cutoffTime, "cutoffTime[16]/D");  
    outTree->Branch("directPeakTimes", &directPeakTimes, "directPeakTimes[16]/D");  
    outTree->Branch("refPeakTimes", &refPeakTimes, "refPeakTimes[16]/D");  

    printf("------------------\n");
    printf("Output File Setup Complete. Begin correlator setup.\n");
    printf("------------------\n");

    printf("Opening file...\n");
    TFile *fp = TFile::Open(inputFile);
    if(!fp) { std::cerr << "Can't open file\n"; return -1; }
    printf("File opened!\n");
    
    //TODO: Implement the code below that differentiates between real and simulated data. 4/6/2024
    
    //Import eventTree
    TTree *eventTree = (TTree*) fp->Get("eventTree");
    printf("Event tree opened!\n");
    if(!eventTree) { std::cerr << "Can't find eventTree\n"; return -1; }
    Long64_t numEntries=eventTree->GetEntries();
    cout << "eventTree has " << numEntries << " entries." << endl;
    RawAtriStationEvent *rawAtriEvPtr=0;
    
    //Grabbing first event to get unixTime and detector info
    // eventTree->SetBranchAddress("UsefulAtriStationEvent", &usefulAtriEvPtr);
    // eventTree->GetEntry(0);
    
    
    

    
    //Try importing paramters using the setup file
    Settings *settings1 = new Settings();
    settings1->ReadFile(setupfile); 
    AraGeomTool *geomTool = AraGeomTool::Instance();
    geomTool->getStationInfo(settings1->DETECTOR_STATION, 2018); 
    
    //Set pedestal file
    AraEventCalibrator *cal = AraEventCalibrator::Instance();
    setPedestalFile(cal, settings1->DETECTOR_STATION, runNum);
    
    IceModel *icemodel=new IceModel(settings1->ICE_MODEL + settings1->NOFZ*10,settings1->CONSTANTICETHICKNESS * 1000 + settings1->CONSTANTCRUST * 100 + settings1->FIXEDELEVATION * 10 + 0,settings1->MOOREBAY);// creates Antarctica ice model
    Detector *detector = new Detector(settings1, icemodel, setupfile);   //This throws the usefulEvent error
    Report *report = new Report(detector, settings1);
    RaySolver *raySolver = new RaySolver;
    //End attempt at importing setup parameters
    
    double dt = settings1->TIMESTEP*1e9;
    

    
    // set up correlator and pairs
    RayTraceCorrelator *theCorrelators[radii.size()];
    for(int r=0; r<radii.size(); r++){
        
        // setup the paths to our ray tracing tables
        double radius = radii[r];
        printf("Loading Radius %.2f\n",radius);
        // double angular_size = 1.;
        int iceModel = settings1->RAY_TRACE_ICE_MODEL_PARAMS; 
        char dirPath[500];
        char refPath[500];
        std::string topDir = "rt_tables";
        sprintf(dirPath, "%s/arrivaltimes_station_%d_icemodel_%d_radius_%.2f_angle_%.2f_solution_0.root",
            topDir.c_str(), station, iceModel, radius, angular_size
        );
        sprintf(refPath, "%s/arrivaltimes_station_%d_icemodel_%d_radius_%.2f_angle_%.2f_solution_1.root",
            topDir.c_str(), station, iceModel, radius, angular_size
        );

        // int numAntennas = 16; //While this should be fine for all stations, it should be dynamic to the settings file as well. 4/6/2024
        int numAntennas = 20; //While this should be fine for all stations, it should be dynamic to the settings file as well. 4/6/2024
        theCorrelators[r] = new RayTraceCorrelator(station, numAntennas, radius, angular_size, dirPath, refPath);
        theCorrelators[r]->LoadTables();        
    }
    
    geomTool->getStationInfo(settings1->DETECTOR_STATION, 2018);

    printf("------------------\n");
    printf("Correlator Setup Complete. Begin looping events.\n");
    printf("------------------\n");    
    
    
    //Use the getTrigMasking function to use the same channels that triggering used for the reconstruction
    std::vector<int> excludedChannels;
    getExcludedChannels(excludedChannels, settings1, detector);
    std::map< int, std::vector<int> > pairs_V = theCorrelators[0]->SetupPairs(station, geomTool, AraAntPol::kVertical, excludedChannels, settings1->DETECTOR_YEAR);  //This throws the usefulEvent error
    std::map< int, std::vector<int> > pairs_H = theCorrelators[0]->SetupPairs(station, geomTool, AraAntPol::kHorizontal, excludedChannels, settings1->DETECTOR_YEAR);    
    
    // Check if sim or real data file by checking for existence of AraTree
    TTree *simSettingsTree;
    TTree *simTree;
    simSettingsTree=(TTree*) fp->Get("AraTree");
    bool dataLike;
    bool calibrated;
    Event *eventPtr = 0; // it is apparently incredibly important that this be initialized to zero...
    Report *reportPtr = 0; 
    std::vector<double> average_position;    
    
    geomTool->getStationInfo(settings1->DETECTOR_STATION, 2018);
    
    //Check for AraTree, as that dictates whether to treat it as simulated or real data.
    if(!simSettingsTree) { 
        dataLike = true;            
        std::cerr << "Can't find AraTree.  Importing as real data.\n";
        //Checks if usefulAtri branch exists.  If not, data gets imported as uncalibrated.
        if (not eventTree->GetBranch("UsefulAtriStationEvent")) {
            calibrated = false;
            cout << "Can't find UsefulAtriStationEvent Tree.  Importing as uncalibrated." << endl;
            eventTree->SetBranchAddress("event",&rawAtriEvPtr);
        }
        else {
            calibrated = true;
            eventTree->SetBranchAddress("UsefulAtriStationEvent", &usefulAtriEvPtr);
        }
        weight = 1;
    }
    // sim like
    else {
        dataLike = false;
        // dataLike = true;  //Setting this to true for troubleshooting.  TODO: fix this
        calibrated = true;
        std::cerr << "AraTree exists.  Importing as simulated data.\n";
        eventTree->SetBranchAddress("UsefulAtriStationEvent", &usefulAtriEvPtr);
        // weight;
        eventTree->SetBranchAddress("weight", &weight);
        simSettingsTree=(TTree*) fp->Get("AraTree");
        if(!simSettingsTree) { std::cerr << "Can't find AraTree\n"; return -1; }
        simSettingsTree->SetBranchAddress("detector", &detector);
        simSettingsTree->GetEntry(0);
        average_position = get_detector_cog(detector);
        printf("Detector Center %.2f, %.2f, %.2f \n", average_position[0], average_position[1], average_position[2]);

        simTree=(TTree*) fp->Get("AraTree2");
        if(!simTree) { std::cerr << "Can't find AraTree2\n"; return -1; }
        simTree->SetBranchAddress("event", &eventPtr);
        simTree->SetBranchAddress("report", &reportPtr);
    }
    
    printf("------------------\n");
    printf("Input files loaded. Set up ray tracing business\n");
    printf("------------------\n");
    
    //Loop over events
    int loopStart;
    int loopEnd;
    if (debugMode) {
        loopStart=debugEvent;
        loopEnd=loopStart+1;
    }
    else {
        loopStart=0;
        loopEnd=numEntries;
    }
    
    for(Long64_t event=loopStart;event<loopEnd;event++) {  
        fp->cd();
        eventTree->GetEntry(event);     
        double pulserDepth;
        double vertexTheta;
        double vertexPhi;
        double vertexRadius;
        double nearestTheta;
        double nearestPhi;
        double nearestRadius;
        
        if (not calibrated) {
            cout << "Triggering uncalibrated data-like condition." << endl;
            delete usefulAtriEvPtr;  //Need to delete the initialized pointer in order to create a new one.
            usefulAtriEvPtr = new UsefulAtriStationEvent(rawAtriEvPtr, AraCalType::kLatestCalib);
        }
        else if (not dataLike) {
            cout << "Triggering sim-like condition." << endl;
            simTree->GetEntry(event);
        }
        
        AraGeomTool *geomTool = AraGeomTool::Instance();
        geomTool->getStationInfo(usefulAtriEvPtr->stationId, usefulAtriEvPtr->unixTime);  
        geomTool->LoadSQLDbAtri(usefulAtriEvPtr->unixTime, usefulAtriEvPtr->stationId);          

        Position diff_true;
        cout << "Calling ara station info!" << endl;
        
        try {
            pulserDepth = getPulserDepth(usefulAtriEvPtr->unixTime);
            cout << "Pulser depth at " << pulserDepth << endl;
        }
        catch(std::out_of_range) {
            cout << "Unix time outside of 2018 SPICE range.  Assuming pulser depth at run number:" << runNum << endl;
        }
        getPulserVertex(detector, settings1, geomTool, pulserDepth, vertexTheta, vertexPhi, vertexRadius, debugMode);
        //Convert vertexTheta and Phi into degrees and same coordiante system as maps
        vertexTheta*=180/PI;
        vertexTheta= -1*vertexTheta + 90;
        vertexPhi*=180/PI;
        if (vertexPhi > 180) {
            vertexPhi-=360;
        }
        cout << "Calculated pulser position at (R, theta[deg], phi[deg]) = (" << vertexRadius << ", " << vertexTheta << ", " << vertexPhi << ")" << endl;   
        
        getNearestVertexBin(vertexRadius, vertexTheta, vertexPhi, angular_size, deltaR, nearestRadius, nearestTheta, nearestPhi);
        cout << "Nearest vertex at (R, theta[deg], phi[deg]) = (" << nearestRadius << ", " << nearestTheta << ", " << nearestPhi << ")" << endl;           
        
        if (dataLike) {
            geomTool->getStationInfo(usefulAtriEvPtr->stationId, usefulAtriEvPtr->unixTime);  
            geomTool->LoadSQLDbAtri(usefulAtriEvPtr->unixTime, usefulAtriEvPtr->stationId);           
            cout << "SQL DB Loaded!" << endl;
        }
        
        //Set event number for output
        if (dataLike) {
            eventNumber = usefulAtriEvPtr->eventNumber;   
        }
        else {
            eventNumber = event;
        }
        if (not dataLike){
            std::vector<double> event_position;
            event_position.push_back(eventPtr->Nu_Interaction[0].posnu.GetX());
            event_position.push_back(eventPtr->Nu_Interaction[0].posnu.GetY());
            event_position.push_back(eventPtr->Nu_Interaction[0].posnu.GetZ());
            printf("Posnu %.2f, %.2f, %.2f \n", event_position[0], event_position[1], event_position[2]);

            std::vector<double> difference;
            difference.push_back(event_position[0] - average_position[0]);
            difference.push_back(event_position[1] - average_position[1]);
            difference.push_back(event_position[2] - average_position[2]);
            printf("Difference %.2f, %.2f, %.2f \n", difference[0], difference[1], difference[2]);

            
            diff_true.SetXYZ(difference[0], difference[1], difference[2]);
            printf("  Vertex Theta %.2f, Phi %.2f, R %.2f\n", diff_true.Theta()*TMath::RadToDeg(), diff_true.Phi()*TMath::RadToDeg(), diff_true.R());            
        }

        std::cout<<"Looking at event number "<<event<<std::endl;        
        
        //TODO: The double peak finder and cropping isn't playing nice with a simulated event with only one peak.
        std::map<int, TGraph*> interpolatedWaveforms;
        std::map<int, TGraph*> interpolatedWaveformsBeforeBandpass;
        std::vector<double> noiseRms(16);
        std::vector<double> noiseRmsBeforeBandpass(16);
        
        TGraph *grCswV;
        TGraph *grCswH;
        
        //TCanvas for the raw waveform separated by channel
        char plotTitle[500];
        sprintf(plotTitle,"A%i Run %s Event %i", station, runNum, eventNumber);        
        TCanvas *c = new TCanvas(plotTitle,plotTitle, 1600, 1600);
        c->Divide(4,4);      
        
        if (not debugMode) {        
            delete c;
        }
        
        if (debugMode) {
            for (int i=0; i<16; i++){
            cout << "RFChan = " << i << " | ElecChan = " << geomTool->getElecChanFromRFChan(i,usefulAtriEvPtr->stationId) << endl;
            }
        }        
        
        //TODO: Implement independent loop for double peak finder that checks parter VPol and HPol channels to find the cutoff times.
        for (int i=0; i<8; i++){
            bool checkExcludedVPol = std::find(excludedChannels.begin(), excludedChannels.end(), i) != excludedChannels.end();
            bool checkExcludedHPol = std::find(excludedChannels.begin(), excludedChannels.end(), i+8) != excludedChannels.end();            
            
            TGraph *grV = usefulAtriEvPtr->getGraphFromRFChan(i);
            TGraph *grH = usefulAtriEvPtr->getGraphFromRFChan(i+8);
            TGraph *grIntV = FFTtools::getInterpolatedGraph(grV, dt);  //Real data interpolation
            TGraph *grIntH = FFTtools::getInterpolatedGraph(grH, dt);  //Real data interpolation
            TGraph *grIntVBeforeBandpass = FFTtools::getInterpolatedGraph(grV, dt);  //Real data interpolation
            TGraph *grIntHBeforeBandpass = FFTtools::getInterpolatedGraph(grH, dt);  //Real data interpolation            
            
            //Apply bandpass filter to eliminate out of band noise
            if (applyBandpass) {
                grIntV = butterworthFilter(grIntV, minFreq, maxFreq, order);
                grIntH = butterworthFilter(grIntH, minFreq, maxFreq, order);
                snrThreshold = 10;
            }
            

            vector<double> vvHitTimes; // vector of hit times
            vector<double> vvPeakIntPowers; // vector of peak values   
            vector<double> hhHitTimes; // vector of hit times
            vector<double> hhPeakIntPowers; // vector of peak values
            vector<double> primaryHitTimes; // vector of hit times
            vector<double> primaryPeakIntPowers; // vector of peak values            
            
            int numSearchPeaks=2; //only look for two peaks  //TODO:  Maybe make this a console argument?  Though the cropping is based on having two peaks 4/10/2024
            getAbsMaximum_N(grIntV, numSearchPeaks, peakSeparation, vvHitTimes, vvPeakIntPowers);
            getAbsMaximum_N(grIntH, numSearchPeaks, peakSeparation, hhHitTimes, hhPeakIntPowers);
            
            double noiseRmsChV = calculateNoiseRMS(grIntV);
            double noiseRmsChH = calculateNoiseRMS(grIntH); 
            double noiseRmsChVBeforeBandpass = calculateNoiseRMS(grIntVBeforeBandpass);
            double noiseRmsChHBeforeBandpass = calculateNoiseRMS(grIntHBeforeBandpass);             
            
            //Compare Vpol and Hpol peaks, choosing the larger peaked channel to serve as the primary peak.
            double peakThresholdV = noiseRmsChV*snrThreshold;
            double peakThresholdH = noiseRmsChH*snrThreshold;
            double peakThreshold;
            // cout << "peakThreshold = " << peakThreshold << endl;
            double firstPeak;
            double secondPeak;
            
            //Adding condition where if the primary peaks are less than their peak thresholds, we grab noise from the end of the waveform and calculate a new threshold.
            if (vvPeakIntPowers[0] < peakThresholdV) {
                noiseRmsChV = calculateNoiseRMS(grIntV, -100);
                noiseRmsChVBeforeBandpass = calculateNoiseRMS(grIntVBeforeBandpass, -100);
                peakThresholdV = noiseRmsChV*4;
            }
            if (hhPeakIntPowers[0] < peakThresholdH) {
                noiseRmsChH = calculateNoiseRMS(grIntH, -100);
                noiseRmsChHBeforeBandpass = calculateNoiseRMS(grIntHBeforeBandpass, -100);
                peakThresholdH = noiseRmsChH*4;
            }            
            
            
            if (vvPeakIntPowers[0]/noiseRmsChV > hhPeakIntPowers[0]/noiseRmsChH) {
                primaryPeakIntPowers = vvPeakIntPowers;
                primaryHitTimes = vvHitTimes;
                peakThreshold = peakThresholdV;
            }
            else {
                primaryPeakIntPowers = hhPeakIntPowers;
                primaryHitTimes = hhHitTimes;
                peakThreshold = peakThresholdH;
            }
                
            noiseRms[i] = noiseRmsChV;
            noiseRms[i+8]=noiseRmsChH;
            noiseRmsBeforeBandpass[i] = noiseRmsChVBeforeBandpass;
            noiseRmsBeforeBandpass[i+8]=noiseRmsChHBeforeBandpass;            
            
            if (primaryPeakIntPowers[1] > peakThreshold and not usefulAtriEvPtr->isCalpulserEvent() and not noCutoff) {            
                cout << "Assuming double-peak signal." << endl;
                if(primaryHitTimes[1]>primaryHitTimes[0]) {
                    firstPeak = primaryHitTimes[0];
                    secondPeak = primaryHitTimes[1];
                } 
                else {
                    firstPeak = primaryHitTimes[1];
                    secondPeak = primaryHitTimes[0];
                }
                cutoffTime[i] = firstPeak + (secondPeak-firstPeak)/2;
                cutoffTime[i+8] = firstPeak + (secondPeak-firstPeak)/2;
                
                //Export peak times to outTree.  Assuming no birefringence at the moment, so V and H signal arrive at the same time.  TODO: Include birefringence in this. 5/14/2024
                directPeakTimes[i] = firstPeak;
                directPeakTimes[i+8] = firstPeak;
                refPeakTimes[i] = secondPeak;
                refPeakTimes[i+8] = secondPeak;                   
            }
            else {
                cout << "Assuming single-peak signal." << endl;                
                cutoffTime[i] = grIntV->GetX()[grIntV->GetN() - 1];
                cutoffTime[i+8] = grIntH->GetX()[grIntH->GetN() - 1];
                
                directPeakTimes[i] = primaryHitTimes[0];
                directPeakTimes[i+8] = primaryHitTimes[0];
                refPeakTimes[i] = cutoffTime[i];
                refPeakTimes[i+8] = cutoffTime[i+8];                    
            }
            if (debugMode){
                //Draw Vpol
                c->cd(i+1); gPad->SetGrid(1,1);
                grIntV->Draw();
                if (checkExcludedVPol) {
                    grV->SetLineColor(2);
                }                
                char vTitle[500];
                sprintf(vTitle,"A%d Run %s Event %d Ch. %.2d %.2f", station, runNum, eventNumber, i, pulserDepth);
                grIntV->SetTitle(vTitle);
                TLine *l1v = new TLine(vvHitTimes[0], -1500, vvHitTimes[0], 1500);
                l1v->SetLineColorAlpha(kBlue, 1);
                l1v->Draw();    
                TLine *l2v = new TLine(vvHitTimes[1], -1500, vvHitTimes[1], 1500);
                l2v->SetLineColorAlpha(kRed, 1);
                l2v->Draw();    
                TLine *l3v = new TLine(-5000, vvPeakIntPowers[0], 5000, vvPeakIntPowers[0]);
                l3v->SetLineColorAlpha(kBlue, 1);
                l3v->Draw();    
                TLine *l4v = new TLine(-5000, vvPeakIntPowers[1], 5000, vvPeakIntPowers[1]);
                l4v->SetLineColorAlpha(kRed, 1);
                l4v->Draw();  
                TLine *l5v = new TLine(-5000, peakThresholdV, 5000, peakThresholdV);
                l5v->SetLineColorAlpha(kGreen, 1);
                l5v->Draw(); 
                TLine *l6v = new TLine(cutoffTime[i], -1500, cutoffTime[i], 1500);
                // l6->SetLineColorAlpha(kPink, 1);
                l6v->SetLineStyle(kDashed);
                l6v->Draw();

                //Draw Hpol
                c->cd(i+1+8); gPad->SetGrid(1,1);
                grIntH->Draw();
                if (checkExcludedHPol) {
                    grIntH->SetLineColor(2);
                }                 
                char hTitle[500];
                sprintf(hTitle,"A%d Run %s Event %d Ch. %.2d %.2f", station, runNum, eventNumber, i+8, pulserDepth);
                // sprintf(hTitle,"A%d Run %s Event %d Ch. %.2d", station, runNum, eventNumber, i+8);
                grIntH->SetTitle(hTitle);
                TLine *l1h = new TLine(hhHitTimes[0], -1500, hhHitTimes[0], 1500);
                l1h->SetLineColorAlpha(kBlue, 1);
                l1h->Draw();    
                TLine *l2h = new TLine(hhHitTimes[1], -1500, hhHitTimes[1], 1500);
                l2h->SetLineColorAlpha(kRed, 1);
                l2h->Draw();    
                TLine *l3h = new TLine(-5000, hhPeakIntPowers[0], 5000, hhPeakIntPowers[0]);
                l3h->SetLineColorAlpha(kBlue, 1);
                l3h->Draw();    
                TLine *l4h = new TLine(-5000, hhPeakIntPowers[1], 5000, hhPeakIntPowers[1]);
                l4h->SetLineColorAlpha(kRed, 1);
                l4h->Draw();  
                TLine *l5h = new TLine(-5000, peakThresholdH, 5000, peakThresholdH);
                l5h->SetLineColorAlpha(kGreen, 1);
                l5h->Draw(); 
                TLine *l6h = new TLine(cutoffTime[i+8], -1500, cutoffTime[i+8], 1500);
                // l6->SetLineColorAlpha(kPink, 1);
                l6h->SetLineStyle(kDashed);
                l6h->Draw();
                
                // grV->GetXaxis()->SetLimits(-100, 50);
                // grH->GetXaxis()->SetLimits(-100, 50);
            }
            
            //Crop waveform to first peak
            if (cutoffTime[i] > grIntV->GetX()[0]) {
                grIntV = FFTtools::cropWave(grIntV, grIntV->GetX()[0], cutoffTime[i]);
            }
            //Either pad or crop waveform to fit NFOUR/2
            if (grIntV->GetN() < settings1->NFOUR/2) {
                grIntV = FFTtools::padWaveToLength(grIntV, settings1->NFOUR/2);
            }
            else if (grIntV->GetN() > settings1->NFOUR/2) {
                grIntV = FFTtools::cropWave(grIntV, grIntV->GetX()[0], grIntV->GetX()[settings1->NFOUR/2-1]);
            }
            interpolatedWaveforms[i] = grIntV;  
            interpolatedWaveformsBeforeBandpass[i] = grIntVBeforeBandpass;  
            
            //Crop waveform to first peak
            if (cutoffTime[i+8] > grIntH->GetX()[0]) {
                grIntH = FFTtools::cropWave(grIntH, grIntH->GetX()[0], cutoffTime[i+8]);
            }
            //Either pad or crop waveform to fit NFOUR/2
            if (grIntH->GetN() < settings1->NFOUR/2) {
                grIntH = FFTtools::padWaveToLength(grIntH, settings1->NFOUR/2);
            }
            else if (grIntH->GetN() > settings1->NFOUR/2) {
                grIntH = FFTtools::cropWave(grIntH, grIntH->GetX()[0], grIntH->GetX()[settings1->NFOUR/2-1]);
            }
            interpolatedWaveforms[i+8] = grIntH; 
            interpolatedWaveformsBeforeBandpass[i+8] = grIntHBeforeBandpass; 
            
            
        }
        

        if (debugMode){      
            char title[500];
            sprintf(title, "%s/waveform.png", outputDir);
            c->Print(title);
        }
    
        std::map<int, double> snrs; // map of channels to SNRs
        for(int i=0; i<16; i++){
            
            double peak_max = TMath::MaxElement(interpolatedWaveforms[i]->GetN(), interpolatedWaveforms[i]->GetY());
            double peak_min = abs(TMath::MinElement(interpolatedWaveforms[i]->GetN(), interpolatedWaveforms[i]->GetY()));
            if(peak_min > peak_max){
                snrs[i] = peak_min/noiseRms[i];
            }
            else{
                snrs[i] = peak_max/noiseRms[i];
            }
        }
        
        std::map<int, double> snrs_fullBand; // map of channels to SNRs
        for(int i=0; i<16; i++){
            
            double peak_max = TMath::MaxElement(interpolatedWaveformsBeforeBandpass[i]->GetN(), interpolatedWaveformsBeforeBandpass[i]->GetY());
            double peak_min = abs(TMath::MinElement(interpolatedWaveformsBeforeBandpass[i]->GetN(), interpolatedWaveformsBeforeBandpass[i]->GetY()));
            if(peak_min > peak_max){
                snrs_fullBand[i] = peak_min/noiseRmsBeforeBandpass[i];
            }
            else{
                snrs_fullBand[i] = peak_max/noiseRmsBeforeBandpass[i];
            }
        }        
        
        // cout << "bbb" << endl;
        std::vector<double> snrs_v;
        std::vector<double> snrs_h;
        for(int i=0; i<8; i++) snrs_v.push_back(snrs[i]);
        for(int i=8; i<16; i++) snrs_h.push_back(snrs[i]);
        // cout << "ccc" << endl;
        //Grabs median-ish snr (third largest of eight) and stores as the VSNR and HSNR. 
        sort(snrs_v.begin(), snrs_v.end(), greater<double>()); // sort largest to smallest
        sort(snrs_h.begin(), snrs_h.end(), greater<double>()); // sort largest to smallest
        double the_snr_v = snrs_v[2];
        double the_snr_h = snrs_h[2];
        
        std::vector<double> snrs_v_fullBand;
        std::vector<double> snrs_h_fullBand;
        for(int i=0; i<8; i++) snrs_v_fullBand.push_back(snrs_fullBand[i]);
        for(int i=8; i<16; i++) snrs_h_fullBand.push_back(snrs_fullBand[i]);
        // cout << "ccc" << endl;
        //Grabs median-ish snr (third largest of eight) and stores as the VSNR and HSNR. 
        sort(snrs_v_fullBand.begin(), snrs_v_fullBand.end(), greater<double>()); // sort largest to smallest
        sort(snrs_h_fullBand.begin(), snrs_h_fullBand.end(), greater<double>()); // sort largest to smallest
        double the_snr_v_fullBand = snrs_v_fullBand[2];
        double the_snr_h_fullBand = snrs_h_fullBand[2];        

        std::map<int, double> weights_V; // V weights
        double tot_weight_v = 0.;
        for(auto iter = pairs_V.begin(); iter!= pairs_V.end(); ++iter){
            int pairNum = iter->first;
            int ant1 = iter->second[0];
            int ant2 = iter->second[1];

            auto gr1_iter = snrs.find(ant1);
            auto gr2_iter = snrs.find(ant2);
            double snr1 = gr1_iter->second;
            double snr2 = gr2_iter->second;
            double snr_prod = snr1 * snr2;
            tot_weight_v+=snr_prod;
            weights_V[pairNum] = snr_prod;
        }

        for(int i=0; i<weights_V.size(); i++){ weights_V[i]/=tot_weight_v; }

        std::map<int, double> weights_H; // H weights
        double tot_weight_h = 0.;
        for(auto iter = pairs_H.begin(); iter!= pairs_H.end(); ++iter){
            int pairNum = iter->first;
            int ant1 = iter->second[0];
            int ant2 = iter->second[1];

            auto gr1_iter = snrs.find(ant1);
            auto gr2_iter = snrs.find(ant2);
            double snr1 = gr1_iter->second;
            double snr2 = gr2_iter->second;
            double snr_prod = snr1 * snr2;
            tot_weight_h+=snr_prod;
            weights_H[pairNum] = snr_prod;
        }
        

        for(int i=0; i<weights_H.size(); i++){ weights_H[i]/=tot_weight_h; }     
        std::vector<TGraph*> corrFunctions_V = theCorrelators[0]->GetCorrFunctions(pairs_V, interpolatedWaveforms, true); 
        std::vector<TGraph*> corrFunctions_H = theCorrelators[0]->GetCorrFunctions(pairs_H, interpolatedWaveforms, true); 

        std::vector<double> peakCorrs;
        std::vector<double> peakThetas;
        std::vector<double> peakPhis;
        std::vector<int> peakPol;
        std::vector<int> peakSol;

        for(int r=0; r<radii.size(); r++){
            double peakCorr, peakTheta, peakPhi;
            if (usePulserCalculation) {
                if (radii[r] != nearestRadius) {
                    continue;
                }
            }
            
                
            std::vector<TH2D*> maps;

            maps.push_back(theCorrelators[r]->GetInterferometricMap(pairs_V, corrFunctions_V, 0, weights_V)); // direct solution)
            maps.push_back(theCorrelators[r]->GetInterferometricMap(pairs_V, corrFunctions_V, 1, weights_V)); // reflected solution
            maps.push_back(theCorrelators[r]->GetInterferometricMap(pairs_H, corrFunctions_H, 0, weights_H)); // direct solution
            maps.push_back(theCorrelators[r]->GetInterferometricMap(pairs_H, corrFunctions_H, 1, weights_H)); // reflected solution           

            std::vector<double> bestOne;
            for(int i=0; i<4; i++){
                double peakCorr, peakTheta, peakPhi;
                getCorrMapPeak(maps[i], peakTheta, peakPhi, peakCorr);   
                bestOne.push_back(peakCorr);
            }
            auto it = max_element(std::begin(bestOne), std::end(bestOne));
            int element = distance(bestOne.begin(), it);
            getCorrMapPeak(maps[element], peakTheta, peakPhi, peakCorr);
            peakCorrs.push_back(peakCorr);

            peakThetas.push_back(peakTheta); 
            peakPhis.push_back(peakPhi);

            if(element < 2){ peakPol.push_back(0); }
            else{peakPol.push_back(1);}

            if(element==0 || element ==2){ peakSol.push_back(0);}
            else if(element==1 || element ==3 ){ peakSol.push_back(1);}

            printf("    Correlated radius %.2f, Corr %.4f \n",radii[r], peakCorr);

            for(int i=0; i<4; i++){
                delete maps[i];
            }
        }

        int element;
        if (usePulserCalculation) {
            auto it = std::find(radii.begin(), radii.end(), nearestRadius);
            element = 0;
        }
        else {
            auto it = max_element(std::begin(peakCorrs), std::end(peakCorrs));
            element = distance(peakCorrs.begin(), it);
        }
        
        // stash the output
        fpOut->cd();
        // reconstructed quantities first
        for(int r=0 ;r<numScanned; r++){
            peakCorrs_out[r] = peakCorrs[r];
            peakThetas_out[r] = peakThetas[r];
            peakPhis_out[r] = peakPhis[r];
            peakPol_out[r] = peakPol[r];
            peakSol_out[r] = peakSol[r];
        }
        if (usePulserCalculation) {
            bestTheta_out = nearestTheta;
            bestPhi_out = nearestPhi;
            bestR_out = nearestRadius; 
            peakSol_out[element] = 0;
        }
        else {
            bestTheta_out = peakThetas[element];
            bestPhi_out = peakPhis[element];
            bestR_out = radii[element];
        }
        bestCorr_out = peakCorrs[element];
        bestSol_out = peakSol_out[element];
        
        
        //Recreate map of best solution and plot it.
        std::vector<TH2D*> maps;

        maps.push_back(theCorrelators[element]->GetInterferometricMap(pairs_V, corrFunctions_V, 0, weights_V)); // direct solution)
        maps.push_back(theCorrelators[element]->GetInterferometricMap(pairs_V, corrFunctions_V, 1, weights_V)); // reflected solution
        maps.push_back(theCorrelators[element]->GetInterferometricMap(pairs_H, corrFunctions_H, 0, weights_H)); // direct solution
        maps.push_back(theCorrelators[element]->GetInterferometricMap(pairs_H, corrFunctions_H, 1, weights_H)); // reflected solution     
        

        if (debugMode){
        
                // Debugging - JCF 6/7/2023
                TCanvas *c = new TCanvas("","", 1200, 950);            
                maps[0]->Draw("colz");
                // maps[0]->GetXaxis()->SetTitle("Phi [deg]");
                // maps[0]->GetYaxis()->SetTitle("Theta [deg]");
                maps[0]->GetXaxis()->SetTitle("Azimuth [degrees]");
                maps[0]->GetYaxis()->SetTitle("Zenith [degrees]");                
                maps[0]->GetYaxis()->SetTitleSize(0.05);
                maps[0]->GetYaxis()->SetLabelSize(0.03);
                // maps[0]->GetYaxis()->SetTitleOffset(0.6);
                maps[0]->GetYaxis()->SetTitleOffset(0.8);
                maps[0]->GetXaxis()->SetTitleSize(0.05);
                maps[0]->GetXaxis()->SetLabelSize(0.03);
                // maps[0]->GetXaxis()->SetTitleOffset(0.6);
                maps[0]->GetXaxis()->SetTitleOffset(0.8);
                gStyle->SetOptStat(0);
                maps[0]->GetXaxis()->CenterTitle();
                maps[0]->GetYaxis()->CenterTitle();
                gPad->SetRightMargin(0.15);
                // Plot calculated pulser location
                TLine *ltheta = new TLine(-180, vertexTheta, 180, vertexTheta);
                ltheta->SetLineColorAlpha(kBlack, 1);
                // ltheta->Draw();  
                TLine *lphi = new TLine(vertexPhi, -90, vertexPhi, 90);
                lphi->SetLineColorAlpha(kBlack, 1);
                // lphi->Draw();            
                char title[500];
                sprintf(title,"%s/bestVpolDsolutionMap.png", outputDir);
                char plotTitle[500];
                // sprintf(plotTitle,"A%d Run %s Event %d Radius %.2f", station, runNum, eventNumber, radii[r]);
                // sprintf(plotTitle,"VPol D A%d Run %s Event %d Radius %f", station, runNum, eventNumber, bestR_out);
                sprintf(plotTitle,"Sample Simulated Neutrino - Station A%d Radius %f", station, bestR_out);            
                maps[0]->SetTitle(plotTitle);
                c->SaveAs(title);
                delete c;
        }
        if (debugMode){

                TCanvas *c = new TCanvas("","", 1200, 950);            
                maps[1]->Draw("colz");
                // maps[0]->GetXaxis()->SetTitle("Phi [deg]");
                // maps[0]->GetYaxis()->SetTitle("Theta [deg]");
                maps[1]->GetXaxis()->SetTitle("Azimuth [degrees]");
                maps[1]->GetYaxis()->SetTitle("Zenith [degrees]");                
                maps[1]->GetYaxis()->SetTitleSize(0.05);
                maps[1]->GetYaxis()->SetLabelSize(0.03);
                // maps[0]->GetYaxis()->SetTitleOffset(0.6);
                maps[1]->GetYaxis()->SetTitleOffset(0.8);
                maps[1]->GetXaxis()->SetTitleSize(0.05);
                maps[1]->GetXaxis()->SetLabelSize(0.03);
                // maps[0]->GetXaxis()->SetTitleOffset(0.6);
                maps[1]->GetXaxis()->SetTitleOffset(0.8);
                gStyle->SetOptStat(0);
                maps[1]->GetXaxis()->CenterTitle();
                maps[1]->GetYaxis()->CenterTitle();
                gPad->SetRightMargin(0.15);
                // Plot calculated pulser location
                TLine *ltheta = new TLine(-180, vertexTheta, 180, vertexTheta);
                ltheta->SetLineColorAlpha(kBlack, 1);
                // ltheta->Draw();  
                TLine *lphi = new TLine(vertexPhi, -90, vertexPhi, 90);
                lphi->SetLineColorAlpha(kBlack, 1);
                // lphi->Draw();             
                char title[500];
                sprintf(title,"%s/bestVpolRsolutionMap.png", outputDir);
                char plotTitle[500];
                // sprintf(plotTitle,"A%d Run %s Event %d Radius %.2f", station, runNum, eventNumber, radii[r]);
                sprintf(plotTitle,"VPol R A%d Run %s Event %d Radius %f", station, runNum, eventNumber, bestR_out);
                maps[1]->SetTitle(plotTitle);
                c->SaveAs(title);
                delete c;
                // End debugging
        }
        
        if (debugMode){
        
                // Debugging - JCF 6/7/2023
                TCanvas *c = new TCanvas("","", 1200, 950);            
                maps[2]->Draw("colz");
                // maps[0]->GetXaxis()->SetTitle("Phi [deg]");
                // maps[0]->GetYaxis()->SetTitle("Theta [deg]");
                maps[2]->GetXaxis()->SetTitle("Azimuth [degrees]");
                maps[2]->GetYaxis()->SetTitle("Zenith [degrees]");                
                maps[2]->GetYaxis()->SetTitleSize(0.05);
                maps[2]->GetYaxis()->SetLabelSize(0.03);
                // maps[0]->GetYaxis()->SetTitleOffset(0.6);
                maps[2]->GetYaxis()->SetTitleOffset(0.8);
                maps[2]->GetXaxis()->SetTitleSize(0.05);
                maps[2]->GetXaxis()->SetLabelSize(0.03);
                // maps[0]->GetXaxis()->SetTitleOffset(0.6);
                maps[2]->GetXaxis()->SetTitleOffset(0.8);
                gStyle->SetOptStat(0);
                maps[2]->GetXaxis()->CenterTitle();
                maps[2]->GetYaxis()->CenterTitle();
                gPad->SetRightMargin(0.15);
                // Plot calculated pulser location
                TLine *ltheta = new TLine(-180, vertexTheta, 180, vertexTheta);
                ltheta->SetLineColorAlpha(kBlack, 1);
                ltheta->Draw();  
                TLine *lphi = new TLine(vertexPhi, -90, vertexPhi, 90);
                lphi->SetLineColorAlpha(kBlack, 1);
                lphi->Draw();             
                char title[500];
                sprintf(title,"%s/bestHpolDsolutionMap.png", outputDir);
                char plotTitle[500];
                // sprintf(plotTitle,"A%d Run %s Event %d Radius %.2f", station, runNum, eventNumber, radii[r]);
                sprintf(plotTitle,"HPol D A%d Run %s Event %d Radius %f", station, runNum, eventNumber, bestR_out);
                maps[2]->SetTitle(plotTitle);
                c->SaveAs(title);
                delete c;
        }
        if (debugMode){

                TCanvas *c = new TCanvas("","", 1200, 950);            
                maps[3]->Draw("colz");
                // maps[0]->GetXaxis()->SetTitle("Phi [deg]");
                // maps[0]->GetYaxis()->SetTitle("Theta [deg]");
                maps[3]->GetXaxis()->SetTitle("Azimuth [degrees]");
                maps[3]->GetYaxis()->SetTitle("Zenith [degrees]");                
                maps[3]->GetYaxis()->SetTitleSize(0.05);
                maps[3]->GetYaxis()->SetLabelSize(0.03);
                // maps[0]->GetYaxis()->SetTitleOffset(0.6);
                maps[3]->GetYaxis()->SetTitleOffset(0.8);
                maps[3]->GetXaxis()->SetTitleSize(0.05);
                maps[3]->GetXaxis()->SetLabelSize(0.03);
                // maps[0]->GetXaxis()->SetTitleOffset(0.6);
                maps[3]->GetXaxis()->SetTitleOffset(0.8);
                gStyle->SetOptStat(0);
                maps[3]->GetXaxis()->CenterTitle();
                maps[3]->GetYaxis()->CenterTitle();
                gPad->SetRightMargin(0.15);
                // Plot calculated pulser location
                TLine *ltheta = new TLine(-180, vertexTheta, 180, vertexTheta);
                ltheta->SetLineColorAlpha(kBlack, 1);
                ltheta->Draw();  
                TLine *lphi = new TLine(vertexPhi, -90, vertexPhi, 90);
                lphi->SetLineColorAlpha(kBlack, 1);
                lphi->Draw();             
                char title[500];
                sprintf(title,"%s/bestHpolRsolutionMap.png", outputDir);
                char plotTitle[500];
                // sprintf(plotTitle,"A%d Run %s Event %d Radius %.2f", station, runNum, eventNumber, radii[r]);
                sprintf(plotTitle,"HPol R A%d Run %s Event %d Radius %f", station, runNum, eventNumber, bestR_out);
                maps[3]->SetTitle(plotTitle);
                c->SaveAs(title);
                delete c;
                // End debugging
        }        
        for(int i=0; i<4; i++){
            delete maps[i];
        }     

        
        // then the MCtruth quantities (if applicable)
        if (not dataLike) {
            int likely_sol = guess_triggering_solution(eventPtr, reportPtr);
            std::map<int, double> thetas_truth = get_value_from_mc_truth("theta", likely_sol, reportPtr);
            std::map<int, double> phis_truth = get_value_from_mc_truth("phi", likely_sol, reportPtr);
            std::map<int, double> launch_thetas_truth = get_value_from_mc_truth("theta_launch", likely_sol, reportPtr);
            std::map<int, double> launch_phis_truth = get_value_from_mc_truth("phi_launch", likely_sol, reportPtr);            
        
            for(int i=0; i<16; i++){
                double this_true_theta = thetas_truth.find(i)->second;
                double this_true_phi = phis_truth.find(i)->second;
                double this_true_launch_theta = launch_thetas_truth.find(i)->second;
                double this_true_launch_phi = launch_phis_truth.find(i)->second;                
                true_arrivalThetas_out[i] = this_true_theta*180/PI;
                true_arrivalPhis_out[i] = this_true_phi*180/PI; 
                true_launchThetas_out[i] = this_true_launch_theta*180/PI;
                true_launchPhis_out[i] = this_true_launch_phi*180/PI;                 
            }
            trueTheta_out = 90 - diff_true.Theta() * TMath::RadToDeg();  //Converted to match the zenith in the reconstruction calculation.
            truePhi_out = (std::fmod((diff_true.Phi() * TMath::RadToDeg())+180,360))-180;
            trueR_out = diff_true.R();
            likelySol_out = likely_sol;
            
            thetas_truth.clear();
            phis_truth.clear();
            launch_thetas_truth.clear();
            launch_phis_truth.clear();
            
        }   

        
        for(int i=0; i<16; i++){

            int this_binTheta, this_binPhi;
            if (useMcTruth) {
                theCorrelators[element]->ConvertAngleToBins(trueTheta_out, truePhi_out, 
                    this_binTheta, this_binPhi);
            }
            // if (usePulserCalculation) {
            //     theCorrelators[element]->ConvertAngleToBins(nearestTheta, nearestPhi, 
            //         this_binTheta, this_binPhi);
            // }
            else {
                theCorrelators[element]->ConvertAngleToBins(bestTheta_out, bestPhi_out, 
                    this_binTheta, this_binPhi);
            }
            double this_arrivalTheta, this_arrivalPhi;
            theCorrelators[element]->LookupArrivalAngles(i, peakSol[element], 
                this_binTheta, this_binPhi,
                this_arrivalTheta, this_arrivalPhi
            );
            
            double this_launchTheta, this_launchPhi;
            theCorrelators[element]->LookupLaunchAngles(i, peakSol[element], 
                this_binTheta, this_binPhi,
                this_launchTheta, this_launchPhi
            );            

            double this_arrivalTime = theCorrelators[element]->LookupArrivalTimes(i, peakSol[element], this_binTheta, this_binPhi);
            reco_arrivalThetas_out[i] = this_arrivalTheta*180/PI; //Previous saved in radians, Converting to degrees - JCF 4/11/2024
            reco_arrivalPhis_out[i] = this_arrivalPhi*180/PI; 
            arrivalTimes_out[i] = this_arrivalTime;
            reco_launchThetas_out[i] = this_launchTheta*180/PI;
            reco_launchPhis_out[i] = this_launchPhi*180/PI;                        
        }

        
        weight_out = weight;
        for(int i=0; i<16; i++){
            snrs_out[i] = snrs[i];
        }
        v_snr_out = the_snr_v;
        h_snr_out = the_snr_h;
        
        for(int i=0; i<16; i++){
            snrs_fullBand_out[i] = snrs_fullBand[i];
        }
        v_snr_fullBand_out = the_snr_v_fullBand;
        h_snr_fullBand_out = the_snr_h_fullBand;        

        outTree->Fill();       

    }

    // write output
    fpOut->Write();
    fpOut->Close();
    delete fpOut;

    cout << "**************************************************" << endl;
    cout << "Output written to:\t " <<outfile_name<<endl;    
    cout << "**************************************************" << endl;

    fp->Close();
    delete fp;
}
