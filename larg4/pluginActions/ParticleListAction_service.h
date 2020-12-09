////////////////////////////////////////////////////////////////////////
/// \file  ParticleListAction_service.h
/// \brief Use Geant4's user "hooks" to maintain a list of particles generated by Geant4.
///
/// \author  seligman@nevis.columbia.edu
////////////////////////////////////////////////////////////////////////
//
// accumulate a list of particles modeled by Geant4.
//

#ifndef PARTICLELISTACTION_SERVICE_H
#define PARTICLELISTACTION_SERVICE_H
// Includes
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "art/Framework/Services/Registry/ServiceMacros.h"
#include "art/Persistency/Provenance/ModuleDescription.h"
#include "canvas/Persistency/Common/Assns.h"

#include "larg4/pluginActions/thePositionInVolumeFilter.h" // larg4::thePositionInVolumeFilter
#include "nug4/ParticleNavigation/ParticleList.h" // larg4::PositionInVolumeFilter
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"
#include "nusimdata/SimulationBase/simb.h" // simb::GeneratedParticleIndex_t
// Get the base classes
#include "artg4tk/actionBase/EventActionBase.hh"
#include "artg4tk/actionBase/TrackingActionBase.hh"
#include "artg4tk/actionBase/SteppingActionBase.hh"

#include "lardataobj/Simulation/GeneratedParticleInfo.h"

#include "Geant4/globals.hh"
#include <map>

// Forward declarations.
class G4Event;
class G4Track;
class G4Step;

namespace sim {
  class ParticleList;
}

namespace larg4 {

  class ParticleListActionService : public  artg4tk::EventActionBase,
                                    public  artg4tk::TrackingActionBase,
                                    public  artg4tk::SteppingActionBase
  {
  public:

    struct ParticleInfo_t {

      simb::MCParticle* particle = nullptr;  ///< simple structure representing particle
      bool keep               = false;        ///< if there was decision to keep
      bool keepFullTrajectory = false;        ///< if there was decision to keep
      /// Index of the particle in the original generator truth record.
      simb::GeneratedParticleIndex_t truthIndex = simb::NoGeneratedParticleIndex;
      /// Resets the information (does not release memory it does not own)
      void clear()
      { particle = nullptr;
        keep = false;
        keepFullTrajectory = false;
        truthIndex = simb::NoGeneratedParticleIndex;
      }

      /// Returns whether there is a particle
      bool hasParticle() const { return particle; }

       /// Returns whether there is a particle
      bool isPrimary() const { return simb::isGeneratedParticleIndex(truthIndex); }

      /// Rerturns whether there is a particle known to be kept
      bool keepParticle() const { return hasParticle() && keep; }

      /// Returns the index of the particle in the generator truth record.
      simb::GeneratedParticleIndex_t truthInfoIndex() const { return truthIndex; }

    }; // ParticleInfo_t

    // Standard constructors and destructors;
    ParticleListActionService(fhicl::ParameterSet const&);
    ~ParticleListActionService();

    // UserActions method that we'll override, to obtain access to
    // Geant4's particle tracks and trajectories.
    virtual void preUserTrackingAction (const G4Track*) override;
    virtual void postUserTrackingAction(const G4Track*) override;
    virtual void userSteppingAction(const G4Step* ) override;

    /// Grabs a particle filter
    void ParticleFilter(std::unique_ptr<thePositionInVolumeFilter>&& filter)
      { fFilter = std::move(filter); }

    // TrackID of the current particle, EveID if the particle is from an EM shower
    static int               GetCurrentTrackID() { return fCurrentTrackID; }

    void                     ResetTrackIDOffset() { fTrackIDOffset = 0;     }

    // Returns the ParticleList accumulated during the current event.
    const sim::ParticleList* GetList() const;

    /// Returns a map of truth record information index for each of the primary
    /// particles (by track ID).
    std::map<int, simb::GeneratedParticleIndex_t> const& GetPrimaryTruthMap() const
      { return fPrimaryTruthMap; }

    /// Returns the index of primary truth (`sim::NoGeneratorIndex` if none).
    simb::GeneratedParticleIndex_t GetPrimaryTruthIndex(int trackId) const;

    // Yields the ParticleList accumulated during the current event.
    sim::ParticleList&& YieldList();

    /// returns whether the specified particle has been marked as dropped
    static bool isDropped(simb::MCParticle const* p);

    // Called at the beginning of each event (note that this is after the
    // primaries have been generated and sent to the event manager)
    void beginOfEventAction(const G4Event* ) override;

    // Called at the end of each event, right before GEANT's state switches
    // out of event processing and into closed geometry (last chance to access
    // the current event).
    void endOfEventAction(const G4Event* ) override;
    // Set/get the current Art event
    void setCurrArtEvent(art::Event & e) { currentArtEvent_ = &e; }
    art::Event  *getCurrArtEvent();
    void  setProductID(art::ProductID pid){pid_=pid;}
    std::unique_ptr <std::vector<simb::MCParticle>>  &GetParticleCollection(){return partCol_;}
    //std::unique_ptr <art::Assns<simb::MCTruth, simb::MCParticle >> &GetAssnsMCTruthToMCParticle(){return tpassn_;}
    std::unique_ptr <art::Assns<simb::MCTruth, simb::MCParticle, sim::GeneratedParticleInfo >> &GetAssnsMCTruthToMCParticle(){return tpassn_;}
  private:
    // A message logger for this action object
    mf::LogInfo logInfo_;

    // this method will loop over the fParentIDMap to get the
    // parentage of the provided trackid
    int                      GetParentage(int trackid) const;

    G4double                 fenergyCut;             ///< The minimum energy for a particle to
                                                     ///< be included in the list.
    ParticleInfo_t           fCurrentParticle;       ///< information about the particle currently being simulated
                                                     ///< for a single particle.
    sim::ParticleList*       fparticleList;          ///< The accumulated particle information for
                                                     ///< all particles in the event.
    G4bool                   fstoreTrajectories;     ///< Whether to store particle trajectories with each particle.
    std::vector<std::string> fkeepGenTrajectories;   ///< List of generators for which fstoreTrejactories applies.
                                                     ///  if not provided and storeTrajectories is true, then all
                                                     ///  trajectories for all generators will be stored. If
                                                     ///  storeTrajectories is set to false, this list is ignored
                                                     ///  and all additional trajectory points are not stored.
    std::map<int, int>       fParentIDMap;           ///< key is current track ID, value is parent ID
    static int               fCurrentTrackID;        ///< track ID of the current particle, set to eve ID
                                                     ///< for EM shower particles
    static int               fTrackIDOffset;         ///< offset added to track ids when running over
                                                     ///< multiple MCTruth objects.
    bool                     fKeepEMShowerDaughters; ///< whether to keep EM shower secondaries, tertiaries, etc
    std::vector<std::string> fNotStoredPhysics;      ///< Physics processes that will not be stored
    bool                     fkeepOnlyPrimaryFullTraj; ///< Whether to store trajectories only for primaries and
                                                       ///  their descendants with MCTruth process = "primary"
    bool                     fSparsifyTrajectories;  ///< help reduce the number of trajectory points.
    double                   fSparsifyMargin;        ///< set the sparsification margin

    std::unique_ptr<thePositionInVolumeFilter> fFilter; ///< filter for particles to be kept

    /// Map: particle track ID -> index of primary information in MC truth.
    std::map<int, simb::GeneratedParticleIndex_t> fPrimaryTruthMap;

    /// Map: particle track ID -> index of primary parent in std::vector<simb::MCTruth> object
    std::map<int, size_t> fMCTIndexMap;

    /// Map: particle trakc ID -> boolean decision to keep or not full trajectory points
    std::map<int,bool> fMCTPrimProcessKeepMap;

    /// Map: MCTruthIndex -> generator, input label of generator and keepGenerator decision
    std::map<size_t, std::pair<std::string, G4bool>> fMCTIndexToGeneratorMap;

    /// Map: not stored process and counter
    std::unordered_map<std::string, int> fNotStoredCounterUMap;

    // Hold on to the current Art event
    art::Event * currentArtEvent_;

    std::unique_ptr<std::vector<simb::MCParticle> > partCol_;
    //std::unique_ptr<art::Assns<simb::MCTruth, simb::MCParticle >> tpassn_;
    std::unique_ptr<art::Assns<simb::MCTruth, simb::MCParticle, sim::GeneratedParticleInfo >> tpassn_;
    art::ProductID pid_;
    /// Adds a trajectory point to the current particle, and runs the filter
    void AddPointToCurrentParticle(TLorentzVector const& pos,
                                   TLorentzVector const& mom,
                                   std::string    const& process);
  };

} // namespace larg4
using larg4::ParticleListActionService;
DECLARE_ART_SERVICE(ParticleListActionService,LEGACY)
#endif // PARTICLELISTACTION_SERVICE_H
