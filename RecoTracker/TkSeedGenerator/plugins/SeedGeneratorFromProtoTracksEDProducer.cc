#include "SeedGeneratorFromProtoTracksEDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "DataFormats/Common/interface/Handle.h"

#include "FWCore/Utilities/interface/InputTag.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/TrajectorySeed/interface/TrajectorySeedCollection.h"

#include "RecoTracker/TkSeedGenerator/interface/SeedFromProtoTrack.h"
#include "RecoTracker/TkSeedingLayers/interface/SeedingHitSet.h"
#include "SeedFromConsecutiveHitsCreator.h"
#include "TrackingTools/TransientTrackingRecHit/interface/TransientTrackingRecHitBuilder.h"
#include "TrackingTools/Records/interface/TransientRecHitRecord.h"
#include "RecoTracker/TkTrackingRegions/interface/GlobalTrackingRegion.h"
#include "TrackingTools/TransientTrackingRecHit/interface/TransientTrackingRecHit.h"

#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"


#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include <vector>

using namespace edm;
using namespace reco;

template <class T> T sqr( T t) {return t*t;}
typedef SeedingHitSet::ConstRecHitPointer Hit;

struct HitLessByRadius { bool operator() (const Hit& h1, const Hit & h2) { return h1->globalPosition().perp2() < h2->globalPosition().perp2(); } };

void SeedGeneratorFromProtoTracksEDProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<InputTag>("InputCollection", InputTag("pixelTracks"));
  desc.add<InputTag>("InputVertexCollection", InputTag(""));
  desc.add<double>("originHalfLength", 1E9);
  desc.add<double>("originRadius", 1E9);
  desc.add<bool>("useProtoTrackKinematics", false);
  desc.add<bool>("useEventsWithNoVertex", true);
  desc.add<std::string>("TTRHBuilder", "TTRHBuilderWithoutAngle4PixelTriplets");
  desc.add<bool>("usePV", false);

  edm::ParameterSetDescription psd0;
  psd0.add<std::string>("ComponentName",std::string("SeedFromConsecutiveHitsCreator"));
  psd0.add<std::string>("propagator",std::string("PropagatorWithMaterial"));
  psd0.add<double>("SeedMomentumForBOFF",5.0);
  psd0.add<double>("OriginTransverseErrorMultiplier",1.0);
  psd0.add<double>("MinOneOverPtError",1.0);
  psd0.add<std::string>("magneticField",std::string(""));
  psd0.add<std::string>("TTRHBuilder",std::string("WithTrackAngle"));
  psd0.add<bool>("forceKinematicWithRegionDirection",false);
  desc.add<edm::ParameterSetDescription>("SeedCreatorPSet",psd0);
  
  descriptions.add("SeedGeneratorFromProtoTracksEDProducer", desc);
}


SeedGeneratorFromProtoTracksEDProducer::SeedGeneratorFromProtoTracksEDProducer(const ParameterSet& cfg)
 : theConfig(cfg)
 , originHalfLength        ( cfg.getParameter<double>("originHalfLength")      )
 , originRadius            ( cfg.getParameter<double>("originRadius")          )
 , useProtoTrackKinematics ( cfg.getParameter<bool>("useProtoTrackKinematics") )
 , useEventsWithNoVertex   ( cfg.getParameter<bool>("useEventsWithNoVertex")   )
 , builderName             ( cfg.getParameter<std::string>("TTRHBuilder")      )
 , usePV_                  ( cfg.getParameter<bool>( "usePV" )                 )
 , theInputCollectionTag       ( consumes<reco::TrackCollection> (cfg.getParameter<InputTag>("InputCollection"))       )
 , theInputVertexCollectionTag ( consumes<reco::VertexCollection>(cfg.getParameter<InputTag>("InputVertexCollection")) )
{
  produces<TrajectorySeedCollection>();
  
  
  
}


void SeedGeneratorFromProtoTracksEDProducer::produce(edm::Event& ev, const edm::EventSetup& es)
{

  std::cout<< " in the main loop " << std::endl;

  auto result = std::make_unique<TrajectorySeedCollection>();
  Handle<reco::TrackCollection> trks;
  ev.getByToken(theInputCollectionTag, trks);

  const TrackCollection &protos = *(trks.product());
  
  edm::Handle<reco::VertexCollection> vertices;
  bool foundVertices = ev.getByToken(theInputVertexCollectionTag, vertices);
  //const reco::VertexCollection & vertices = *(h_vertices.product());

  ///
  /// need optimization: all es stuff should go out of the loop
  /// 
  for (TrackCollection::const_iterator it=protos.begin(); it!= protos.end(); ++it) {

    std::cout<< " in the TrackCollection loop " << std::endl;
    const Track & proto = (*it);
    GlobalPoint vtx(proto.vertex().x(), proto.vertex().y(), proto.vertex().z());

    // check the compatibility with a primary vertex
    bool keepTrack = false;
    if ( (!foundVertices) || vertices->empty() ) {
      std::cout<< " check vertices loop " << std::endl; 
      if (useEventsWithNoVertex) keepTrack = true;
      std::cout<< " end of check vertices loop " << std::endl;
    } 
    else if (usePV_){
       
      std::cout<< " use PV loop " << std::endl;
 
      GlobalPoint aPV(vertices->begin()->position().x(),vertices->begin()->position().y(),vertices->begin()->position().z());
      double distR2 = sqr(vtx.x()-aPV.x()) +sqr(vtx.y()-aPV.y());
      double distZ = fabs(vtx.z()-aPV.z());
      if ( distR2 < sqr(originRadius) && distZ < originHalfLength ) {
        keepTrack = true;
      }
      std::cout<< " end of use PV loop " << std::endl;
    }
    else { 
      for (reco::VertexCollection::const_iterator iv=vertices->begin(); iv!= vertices->end(); ++iv) {
        std::cout<< " reco vertex collection loop " << std::endl;
        GlobalPoint aPV(iv->position().x(),iv->position().y(),iv->position().z());
	double distR2 = sqr(vtx.x()-aPV.x()) +sqr(vtx.y()-aPV.y());
	double distZ = fabs(vtx.z()-aPV.z());
	if ( distR2 < sqr(originRadius) && distZ < originHalfLength ) { 
	  keepTrack = true;
	  break;
          std::cout<< " code breaks " << std::endl;
        }
      }
    }
     std::cout<< " track found !! " << std::endl;
    if (!keepTrack) continue;

    if ( useProtoTrackKinematics ) {
      std::cout<< " useProtoTrackKinematics loop " << std::endl;
      SeedFromProtoTrack seedFromProtoTrack( proto, es);
      if (seedFromProtoTrack.isValid()) (*result).push_back( seedFromProtoTrack.trajectorySeed() );
      std::cout<< " seedFromProtoTrack.isValid() loop " << std::endl; 
    } else {
       std::cout<< " seedFromProtoTrack.is NOT Valid() loop " << std::endl;
      edm::ESHandle<TransientTrackingRecHitBuilder> ttrhbESH;
      es.get<TransientRecHitRecord>().get(builderName,ttrhbESH);
      std::vector<Hit> hits;
      for (unsigned int iHit = 0, nHits = proto.recHitsSize(); iHit < nHits; ++iHit) {

        std::cout<< " inside recHit loop " << std::endl; 
        TrackingRecHitRef refHit = proto.recHit(iHit);
        if(refHit->isValid()) hits.push_back((Hit)&(*refHit));
          
      }
      sort(hits.begin(), hits.end(), HitLessByRadius());

      if (hits.size() > 1) {
        std::cout<< " hits.size() > 1 loop " << std::endl;
        double mom_perp = sqrt(proto.momentum().x()*proto.momentum().x()+proto.momentum().y()*proto.momentum().y());
        std::cout<< " hits.size() > 1 loop 1" << std::endl;
	GlobalTrackingRegion region(mom_perp, vtx, 0.2, 0.2);
        std::cout<< " hits.size() > 1 loop 2" << std::endl;
	edm::ParameterSet seedCreatorPSet = theConfig.getParameter<edm::ParameterSet>("SeedCreatorPSet");
        std::cout<< " hits.size() > 1 loop 3" << std::endl;
	SeedFromConsecutiveHitsCreator seedCreator(seedCreatorPSet);
        std::cout<< " hits.size() > 1 loop 4" << std::endl;
	seedCreator.init(region, es, nullptr);
        std::cout<< " hits.size() > 1 loop 5" << std::endl;  
	seedCreator.makeSeed(*result, SeedingHitSet(hits[0], hits[1], hits.size() >2 ? hits[2] : SeedingHitSet::nullPtr() ));
        std::cout<< " hits.size() > 1 loop 6" << std::endl;
      }
    }
  } 

  ev.put(std::move(result));
  std::cout<< " end of TrackCollection loop " << std::endl;
}

