import FWCore.ParameterSet.Config as cms
process = cms.Process("ProcessOne")

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
                            #firstRun = cms.untracked.uint32(315257),
                            firstRun = cms.untracked.uint32(320500),
                            numberEventsInRun    = cms.untracked.uint32(2000),
                            firstLuminosityBlock = cms.untracked.uint32(1),
                            numberEventsInLuminosityBlock = cms.untracked.uint32(1),
                            )

#process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(25000000))
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(1000000))

##
## TrackerTopology
##
process.load("Configuration.Geometry.GeometryExtended2017_cff")
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
                                                          #tag = cms.string("SiPixelQuality_byPCL_stuckTBM_v1")
                                                          #tag = cms.string("SiPixelQuality_byPCL_other_v1")
                                                          tag = cms.string("SiPixelQuality_byPCL_prompt_v2")
                                                          )
                                                 )
                               )

process.ReadInDB = cms.EDAnalyzer("SiPixelQualityPlotter",
                                  lumiInputTag = cms.untracked.InputTag("LumiInfo", "brilcalc")
)

process.LumiInfo = cms.EDProducer('LumiProducerFromBrilcalc',
                                  lumiFile = cms.string("./luminosityDB_2018.csv"),
                                  throwIfNotFound = cms.bool(False),
                                  doBunchByBunch = cms.bool(False))


process.p = cms.Path(process.LumiInfo*process.ReadInDB)
