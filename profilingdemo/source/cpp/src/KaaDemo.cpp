/**
 *  Copyright 2014-2016 CyberVision, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <kaa/Kaa.hpp>
#include <kaa/IKaaClient.hpp>
#include <kaa/failover/IFailoverStrategy.hpp>
#include <kaa/failover/DefaultFailoverStrategy.hpp>
#include <kaa/profile/IProfileContainer.hpp>
#include <kaa/profile/DefaultProfileContainer.hpp>
#include <kaa/configuration/manager/IConfigurationReceiver.hpp>

#include <boost/filesystem.hpp>

#include <vector>

using namespace kaa;

class ProfileData
{
public:
    ProfileData(bool audioSupport, bool videoSupport, 
            bool vibroSupport)
    {
        profile_.audioSubscrpitionActive = audioSupport;
        profile_.videoSubbscrptionActive = videoSupport;
        profile_.vibroSubscriptionActive = vibroSupport;
    }

    ~ProfileData() = default;

    const KaaProfile &getProfile() const
    {
        return profile_;
    }

private:
    KaaProfile profile_;
};

static const std::vector<ProfileData> clientProfiles = {
    ProfileData(true, false, true),
    ProfileData(false, false, true),
    ProfileData(true, true, true),
    ProfileData(true, true, false),
};

typedef std::shared_ptr<IKaaClient> IKaaClientPtr;

class ConfigurationListener: public IConfigurationReceiver
{
public:
    ConfigurationListener(IKaaClientPtr kaaClient):
        kaaClient_(kaaClient)
    { 
        if (!kaaClient_) {
            throw std::invalid_argument("KaaClient is null");
        }
    }

    ~ConfigurationListener() = default;

    virtual void onConfigurationUpdated(const KaaRootConfiguration &configuration)
    {
        // TODO: merge KAA-799
        // std::cout << "Endpoint Key Hash: " << kaaClient_->getEndpointKeyHash() << std::endl;
        std::cout << "Audio Support: " << configuration.audioSubscriptionActive << std::endl;
        std::cout << "Video Support: " << configuration.videoSubscriptionActive << std::endl;
        std::cout << "Vibro Support: " << configuration.vibroSuvscrpitionActive << std::endl;
    }

private:
    IKaaClientPtr kaaClient_;
};

typedef std::shared_ptr<ConfigurationListener> ConfigurationListenerPtr;

class KaaClientManager
{
public:
    KaaClientManager() = default;

    ~KaaClientManager()
    {
        for (auto &client : kaaClients_) {
            client->stop();
        }
    }

    void spawnKaaClient(const KaaProfile &profile)
    {
        std::string clientDir = "client" + std::to_string(kaaClients_.size());
        boost::filesystem::path dir(clientDir);

        if (!boost::filesystem::create_directory(dir)) {
            std::cerr << "Failed to create directory " << dir.c_str() << std::endl;
            kaaClients_.pop_back();
            return;
        }

        IKaaClientPlatformContextPtr clientContext
            = std::make_shared<KaaClientPlatformContext>();

        auto &clientProperties = clientContext->getProperties();
        clientProperties.setWorkingDirectoryPath(clientDir);

        auto kaaClient = Kaa::newClient(clientContext);
        kaaClients_.push_back(kaaClient);
        
        auto configurationListener =
            std::make_shared<ConfigurationListener>(ConfigurationListener(kaaClient));
        kaaClient->addConfigurationListener(*configurationListener);
        configurationListeners_.push_back(configurationListener);

        kaaClient->start();
        
        auto profileContainer =
            std::make_shared<DefaultProfileContainer>(DefaultProfileContainer(profile));
        kaaClient->setProfileContainer(profileContainer);
        kaaClient->updateProfile();
    }

private:
    std::vector<IKaaClientPtr> kaaClients_;
    std::vector<ConfigurationListenerPtr> configurationListeners_;
};

int main()
{
    KaaClientManager clientManager;

    for (const auto &profile : clientProfiles) {
        clientManager.spawnKaaClient(profile.getProfile());
    }

    std::cout << "Spawned " << clientProfiles.size() << " clients" << std::endl;

    std::cout << "Press any key to exit" << std::endl;

    std::cin.get();

    return 0;
}