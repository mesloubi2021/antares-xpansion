#include "ActiveLinks.h"
#include <unordered_set>
#include <limits>

bool doubles_are_different(const double a,
                           const double b) {
    constexpr double MACHINE_EPSILON = std::numeric_limits<double>::epsilon();
    return std::abs(a - b) > MACHINE_EPSILON;
}


void ActiveLinksBuilder::addCandidate(const CandidateData &candidate_data) {

    unsigned int indexLink = getLinkIndexOf(candidate_data.link_id);
    _links.at(indexLink).addCandidate(candidate_data, getProfileFromProfileMap(candidate_data.link_profile));
}

ActiveLinksBuilder::ActiveLinksBuilder(const std::vector<CandidateData>& candidateList, const std::map<std::string, LinkProfile>& profile_map):
    _candidateDatas(candidateList), _profile_map(profile_map)
{
    checkCandidateNameDuplication();
    checkLinksValidity();
}

void ActiveLinksBuilder::checkLinksValidity()
{
    for (const auto& candidateData : _candidateDatas)
    {
        launchExceptionIfNoLinkProfileAssociated(candidateData.link_profile);
        launchExceptionIfNoLinkProfileAssociated(candidateData.installed_link_profile_name);

        record_link_data(candidateData);
    }
}

void ActiveLinksBuilder::record_link_data(const CandidateData &candidateData) {
    LinkData link_data = {candidateData.link_id, candidateData.already_installed_capacity, candidateData.installed_link_profile_name, candidateData.linkor, candidateData.linkex};
    const auto & it = _links_data.find(candidateData.link_name);
    if(it == _links_data.end()){
        _links_data[candidateData.link_name] = link_data;
    } else{
        const LinkName link_name = it->first;
        raise_errors_if_link_data_differs_from_existing_link(link_data, link_name);
    }
}

void ActiveLinksBuilder::raise_errors_if_link_data_differs_from_existing_link(const ActiveLinksBuilder::LinkData &link_data,
                                                                              const LinkName &link_name) const {
    const LinkData & old_link_data =  _links_data.at(link_name);
    if(doubles_are_different(link_data.installed_capacity, old_link_data.installed_capacity)){
        std::string message = "Multiple already installed capacity detected for link " + link_name;
        throw std::runtime_error(message);
    }
    if(old_link_data.profile_name != link_data.profile_name){
        std::string message = "Multiple already_installed_profile detected for link " + link_name;
        throw std::runtime_error(message);
    }
    if(old_link_data.id != link_data.id){
        std::string message = "Multiple link_id detected for link " + link_name;
        throw std::runtime_error(message);
    }
}


void ActiveLinksBuilder::launchExceptionIfNoLinkProfileAssociated(const std::string& profileName) const
{
    if (!profileName.empty()){
        const auto& it_profile = _profile_map.find(profileName);

        if (it_profile == _profile_map.end())
        {
            std::string message = "There is no linkProfile associated with " + profileName;
            throw std::runtime_error(message);
        }
    }
    
}

void ActiveLinksBuilder::checkCandidateNameDuplication() const
{
    std::unordered_set<std::string> setCandidatesNames;
    for (const auto& candidateData : _candidateDatas){
        auto it_inserted = setCandidatesNames.insert(candidateData.name);
        if (!it_inserted.second){
            std::string message = "Candidate " + candidateData.name + " duplication detected";
            throw std::runtime_error(message);
        }
    }
}

const std::vector<ActiveLink>& ActiveLinksBuilder::getLinks()
{
    if (_links.empty()){
        create_links();
        for (const CandidateData& candidateData : _candidateDatas) {
            if  (candidateData.enable){
                addCandidate(candidateData);
            }
        }
    }
    return _links;
}

int ActiveLinksBuilder::getLinkIndexOf(int link_id) const
{
    int index = -1;
    for (int i = 0; i <_links.size(); i++) {
        if (_links.at(i).get_idLink() == link_id){
            index = i;
            break;
        }
    }
    return index;
}

void ActiveLinksBuilder::create_links() {
    for( auto const & it: _links_data){
        LinkName name = it.first;
        LinkData data = it.second;
        ActiveLink link(data.id, name, data._linkor, data._linkex, data.installed_capacity);
        link.setAlreadyInstalledLinkProfile(getProfileFromProfileMap(data.profile_name));
        _links.push_back(link);
    }
}

LinkProfile ActiveLinksBuilder::getProfileFromProfileMap(const std::string &profile_name) const {
    LinkProfile already_installed_link_profile;
    if(_profile_map.find(profile_name) != _profile_map.end()){
        already_installed_link_profile = _profile_map.at(profile_name);
    }
    return already_installed_link_profile;
}

ActiveLink::ActiveLink(int idLink, const std::string &linkName, const std::string &linkor, const std::string &linkex,
                       const double &already_installed_capacity) :
    _idLink(idLink), _name(linkName),
    _linkor(linkor), _linkex(linkex),
    _already_installed_capacity(already_installed_capacity)
{
}

void ActiveLink::setAlreadyInstalledLinkProfile(const LinkProfile& linkProfile)
{
    _already_installed_profile = linkProfile;
}

void ActiveLink::addCandidate(const CandidateData& candidate_data, const LinkProfile& candidate_profile)
{
    Candidate candidate(candidate_data, candidate_profile);

    _candidates.push_back(candidate);
}

const std::vector<Candidate>& ActiveLink::getCandidates() const
{
    return _candidates;
}

double ActiveLink::already_installed_direct_profile(size_t timeStep) const
{
    return _already_installed_profile.getDirectProfile(timeStep);
}

double ActiveLink::already_installed_indirect_profile(size_t timeStep) const
{
    return _already_installed_profile.getIndirectProfile(timeStep);
}


int ActiveLink::get_idLink() const {
    return _idLink; 	
}

LinkName ActiveLink::get_LinkName() const{
    return _name;
}

double ActiveLink::get_already_installed_capacity() const{
    return _already_installed_capacity;
}

std::string ActiveLink::get_linkor() const{
    return _linkor;
}

std::string ActiveLink::get_linkex() const{
    return _linkex;
}
