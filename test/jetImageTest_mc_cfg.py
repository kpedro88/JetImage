from FWCore.ParameterSet.VarParsing import VarParsing
import FWCore.ParameterSet.Config as cms
import os, sys, json

options = VarParsing("analysis")
options.register("address", "ailab01.fnal.gov", VarParsing.multiplicity.singleton, VarParsing.varType.string)
options.register("port", 8001, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.register("timeout", 30, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.register("params", "", VarParsing.multiplicity.singleton, VarParsing.varType.string)
options.register("threads", 1, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.register("streams", 0, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.register("batchSize", 10, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.register("modelName","resnet50_netdef", VarParsing.multiplicity.singleton, VarParsing.varType.string)
options.register("mode","PseudoAsync", VarParsing.multiplicity.singleton, VarParsing.varType.string)
options.register("verbose", False, VarParsing.multiplicity.singleton, VarParsing.varType.bool)
options.parseArguments()

if len(options.params)>0:
    with open(options.params,'r') as pfile:
        pdict = json.load(pfile)
    options.address = pdict["address"]
    options.port = int(pdict["port"])
    print("server = "+options.address+":"+str(options.port))

# check mode
allowed_modes = {
    "Async": "JetImageProducerAsync",
    "Sync": "JetImageProducerSync",
    "PseudoAsync": "JetImageProducerPseudoAsync",
}
if options.mode not in allowed_modes:
    raise ValueError("Unknown mode: "+options.mode)

process = cms.Process('imageTest')

process.load('FWCore/MessageService/MessageLogger_cfi')
process.load('Configuration/StandardSequences/GeometryRecoDB_cff')
process.load('Configuration/StandardSequences/MagneticField_38T_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')

from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, 'auto:phase1_2018_realistic', '')

process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(options.maxEvents) )
process.source = cms.Source("PoolSource",
    fileNames = cms.untracked.vstring([
        '/store/mc/RunIIAutumn18MiniAOD/ZprimeToTTJet_M3000_TuneCP2_13TeV-madgraph-pythia8/MINIAODSIM/102X_upgrade2018_realistic_v15-v1/120000/8161D81E-8273-4941-A813-5815F918C909.root',
    ]),
	duplicateCheckMode = cms.untracked.string('noDuplicateCheck')
)

if len(options.inputFiles)>0: process.source.fileNames = options.inputFiles

# enforce consistent input size
_ncol = 3
_npix = 224
process.jetImageProducer = cms.EDProducer(allowed_modes[options.mode],
    JetTag = cms.InputTag('slimmedJetsAK8'),
    topN = cms.uint32(5),
    ncol = cms.uint32(_ncol),
    npix = cms.uint32(_npix),
    imageList = cms.string("../data/imagenet_classes.txt"),
    Client = cms.PSet(
        nInput  = cms.uint32(_npix*_npix*_ncol),
        nOutput = cms.uint32(1000),
        batchSize = cms.untracked.uint32(options.batchSize),
        address = cms.untracked.string(options.address),
        port = cms.untracked.uint32(options.port),
        timeout = cms.untracked.uint32(options.timeout),
        modelName = cms.string(options.modelName),
        modelVersion = cms.int32(-1),
        verbose = cms.untracked.bool(options.verbose),
        allowedTries = cms.untracked.uint32(0),
    )
)

# Let it run
process.p = cms.Path(
    process.jetImageProducer
)

process.MessageLogger.cerr.FwkReport.reportEvery = 500
keep_msgs = ['JetImageProducer','JetImageProducer:TritonClient']
for msg in keep_msgs:
    process.MessageLogger.categories.append(msg)
    setattr(process.MessageLogger.cerr,msg,
        cms.untracked.PSet(
            optionalPSet = cms.untracked.bool(True),
            limit = cms.untracked.int32(10000000),
        )
    )

if options.threads>0:
    if not hasattr(process,"options"): process.options = cms.untracked.PSet()
    process.options.numberOfThreads = cms.untracked.uint32(options.threads)
    process.options.numberOfStreams = cms.untracked.uint32(options.streams if options.streams>0 else 0)
