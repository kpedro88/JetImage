#include "HeterogeneousCore/SonicCore/interface/SonicEDProducer.h"
#include "HeterogeneousCore/SonicTriton/interface/TritonClient.h"

#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "DataFormats/Math/interface/deltaPhi.h"
#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/JetReco/interface/Jet.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidate.h"

#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

template <typename Client>
class JetImageProducer : public SonicEDProducer<Client>
{
	public:
		//needed because base class has dependent scope
		using typename SonicEDProducer<Client>::Input;
		using typename SonicEDProducer<Client>::Output;
		explicit JetImageProducer(edm::ParameterSet const& cfg) :
			SonicEDProducer<Client>(cfg),
			JetTag_(cfg.getParameter<edm::InputTag>("JetTag")),
			JetTok_(this->template consumes<edm::View<pat::Jet>>(JetTag_)),
			npix_(cfg.getParameter<unsigned>("npix")),
			ncol_(cfg.getParameter<unsigned>("ncol")),
			topN_(cfg.getParameter<unsigned>("topN"))
		{
			//for debugging
			this->setDebugName("JetImageProducer");
			//load score list
			std::string imageListFile(cfg.getParameter<std::string>("imageList"));
			std::ifstream ifile(imageListFile);
			if(ifile.is_open()){
				std::string line;
				while(std::getline(ifile,line)){
					imageList_.push_back(line);
				}
			}
			else {
				throw cms::Exception("MissingFile") << "Could not open image list file: " << imageListFile;
			}
			//safety check
			float dim = npix_*npix_*ncol_;
			if(dim != client_.nInput()){
				throw cms::Exception("InconsistentParameters") << "Client nInput = " << client_.nInput() << " != " << npix_ << "*" << npix_ << "*" << ncol_ << " = " << dim;
			}
		}
		void acquire(edm::Event const& iEvent, edm::EventSetup const& iSetup, Input& iInput) override {
			//input data from event
			edm::Handle<edm::View<pat::Jet>> h_jets;
			iEvent.getByToken(JetTok_, h_jets);
			const auto& jets = *h_jets.product();

			// create a jet image for each jet in the event
			// npix x npix x ncol image which is centered at the jet axis and +/- 1 unit in eta and phi
			// set batch size to correspond to number of jets
			client_.setBatchSize(jets.size());
			iInput.resize(client_.nInput()*client_.batchSize(),0.f);

			float pixel_width = 2./float(npix_);

			int jet_ctr = 0;
			for(const auto& i_jet : jets){
				//jet calcs
				float jet_pt  =  i_jet.pt();
				float jet_phi =  i_jet.phi();
				float jet_eta =  i_jet.eta();

				for(unsigned k = 0; k < i_jet.numberOfDaughters(); ++k){
					const reco::Candidate* i_part = i_jet.daughter(k);
					// daughter info
					float i_pt = i_part->pt();
					float i_phi = i_part->phi();
					float i_eta = i_part->eta();

					float dphi = reco::deltaPhi(i_phi,jet_phi);
					float deta = i_eta - jet_eta;

					// outside of the image, shouldn't happen for AK8 jet!
					if ( deta > 1. || deta < -1. || dphi > 1. || dphi < -1.) continue;
					int eta_pixel_index =  (int) ((deta + 1.)/pixel_width);
					int phi_pixel_index =  (int) ((dphi + 1.)/pixel_width);
					int index_base = ncol_*(eta_pixel_index*npix_+phi_pixel_index) + jet_ctr*client_.nInput();
					iInput[index_base+0] += i_pt/jet_pt;
					iInput[index_base+1] += i_pt/jet_pt;
					iInput[index_base+2] += i_pt/jet_pt;
				}

				jet_ctr++;
			}
		}
		void produce(edm::Event& iEvent, edm::EventSetup const& iSetup, Output const& iOutput) override {
			//check the results
			findTopN(iOutput);
		}
		~JetImageProducer() override {}

		static void fillDescriptions(edm::ConfigurationDescriptions & descriptions) {
			edm::ParameterSetDescription desc;
			Client::fillPSetDescription(desc);
			desc.add<edm::InputTag>("JetTag",edm::InputTag("slimmedJetsAK8"));
			desc.add<unsigned>("topN",5);
			desc.add<unsigned>("npix",224);
			desc.add<unsigned>("ncol",3);
			desc.add<std::string>("imageList");
			//to ensure distinct cfi names
			descriptions.addWithDefaultLabel(desc);
		}

	private:
		using SonicEDProducer<Client>::client_;
		void findTopN(const std::vector<float>& scores, unsigned n=5) const {
			auto dim = client_.nOutput();
			for(unsigned i0 = 0; i0 < client_.batchSize(); i0++) {
				//match score to type by index, then put in largest-first map
				std::map<float,std::string,std::greater<float>> score_map;
				for(unsigned i = 0; i < std::min((unsigned)dim,(unsigned)imageList_.size()); ++i){
					score_map.emplace(scores[i0*dim+i],imageList_[i]);
				}
				//get top n
				std::stringstream msg;
				msg << "Scores:\n";
				unsigned counter = 0;
				for(const auto& item: score_map){
					msg << item.second << " : " << item.first << "\n";
					++counter;
					if(counter>=topN_) break;
				}
				edm::LogInfo(client_.debugName()) << msg.str();
			}
		}

		edm::InputTag JetTag_;
		edm::EDGetTokenT<edm::View<pat::Jet>> JetTok_;
		unsigned npix_, ncol_;
		unsigned topN_;
		std::vector<std::string> imageList_;
};

typedef JetImageProducer<TritonClientSync> JetImageProducerSync;
typedef JetImageProducer<TritonClientAsync> JetImageProducerAsync;
typedef JetImageProducer<TritonClientPseudoAsync> JetImageProducerPseudoAsync;

DEFINE_FWK_MODULE(JetImageProducerSync);
DEFINE_FWK_MODULE(JetImageProducerAsync);
DEFINE_FWK_MODULE(JetImageProducerPseudoAsync);
