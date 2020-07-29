from __future__ import print_function
import shlex, shutil, getpass
import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

process = cms.Process("ProcessOne")

options = VarParsing.VarParsing("analysis")
options.register ('firstRun',
                  1,VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.int,              # string, int, or float
                  "first run to be processed")
options.register ('nLSToProcessPerRun',
                  1,VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.int,              # string, int, or float
                  "number of lumisections to process per run")
options.register ('nRunsToProcess',
                  1,VarParsing.VarParsing.multiplicity.singleton, # singleton or list
                  VarParsing.VarParsing.varType.int,              # string, int, or float
                  "total number of Runs to process")
options.parseArguments()

##
## MessageLogger
##
process.load('FWCore.MessageService.MessageLogger_cfi')   
process.MessageLogger.categories.append("SiPixelQualityPlotter")  
#process.MessageLogger.categories.append("LumiProducerFromBrilcalc")  
process.MessageLogger.destinations = cms.untracked.vstring("cout")
process.MessageLogger.cout = cms.untracked.PSet(
    threshold = cms.untracked.string("INFO"),
    default   = cms.untracked.PSet(limit = cms.untracked.int32(0)),                       
    FwkReport = cms.untracked.PSet(limit = cms.untracked.int32(-1),
                                   reportEvery = cms.untracked.int32(1000)
                                   ),                                                      
    SiPixelQualityPlotter = cms.untracked.PSet( limit = cms.untracked.int32(-1)),
    #LumiProducerFromBrilcalc = cms.untracked.PSet( limit = cms.untracked.int32(-1))
    )
process.MessageLogger.statistics.append('cout')  

##
## Empty source
##
process.source = cms.Source("EmptySource",
                            firstRun = cms.untracked.uint32(options.firstRun),
                            numberEventsInRun    = cms.untracked.uint32(options.nLSToProcessPerRun),
                            firstLuminosityBlock = cms.untracked.uint32(1),
                            numberEventsInLuminosityBlock = cms.untracked.uint32(1),
                            )

maxEventsToProcess = (options.nRunsToProcess*options.nLSToProcessPerRun) + 1       # the +1 is needed in order to fall on the next run (outside of the range selected and close the last IOV
maxRunsToProcess = (options.firstRun+options.nRunsToProcess)-1
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(maxEventsToProcess))

## 
## Now do some printing
##

print("===============================================")
print(" First run to be processed: ",options.firstRun)
print(" Last  run to be processed: ",maxRunsToProcess)
print(" n. LS process per Run: ",options.nLSToProcessPerRun)
print(" Total LS to process: ",maxEventsToProcess)
print("===============================================")

##
## TrackerTopology
##
process.load("Configuration.Geometry.GeometryExtended2017_cff")         # loads the Phase-1 topology
process.load("Geometry.TrackerGeometryBuilder.trackerParameters_cfi")
process.TrackerTopologyEP = cms.ESProducer("TrackerTopologyEP")

##
## Database output service
##
process.load("CondCore.CondDB.CondDB_cfi")

# DB input service: 
process.CondDB.connect = "frontier://FrontierProd/CMS_CONDITIONS"
process.dbInput = cms.ESSource("PoolDBESSource",
                               process.CondDB,
                               toGet = cms.VPSet(cms.PSet(record = cms.string("SiPixelQualityFromDbRcd"),
                                                          tag = cms.string("SiPixelQuality_byPCL_prompt_v2") ## Prompt
                                                          #tag = cms.string("SiPixelQuality_v07_offline")      ## Re-Reco
                                                          )
                                                 )
                               )
##
## Clean old files
##
chosenTag=process.dbInput.toGet[0].tag.value()
try:
    shutil.move("SummaryBarrel_"+chosenTag+".png", "SummaryBarrel_"+chosenTag+"_old.png")
    shutil.move("SummaryForward_"+chosenTag+".png", "SummaryForward_"+chosenTag+"_old.png")
except:
    print("No old files to be moved")

##
## The analysis module
##
process.ReadInDB = cms.EDAnalyzer("SiPixelQualityPlotter",
                                  analyzedTag = cms.string(chosenTag),
                                  maxRun = cms.untracked.uint32(maxRunsToProcess),
                                  lumiInputTag = cms.untracked.InputTag("LumiInfo", "brilcalc")
                                  )

##
## The lumi information
##
process.LumiInfo = cms.EDProducer('LumiProducerFromBrilcalc',
                                  lumiFile = cms.string("./luminosityDB.csv"),
                                  throwIfNotFound = cms.bool(False),
                                  doBunchByBunch = cms.bool(False))

##
## The path
##
process.p = cms.Path(process.LumiInfo*process.ReadInDB)
