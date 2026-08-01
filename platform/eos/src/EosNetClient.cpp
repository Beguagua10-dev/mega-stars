#include <cstdio>

#include "mega/net/NetClient.h"

#ifdef MEGA_WITH_EOS
#include <eos_sdk.h>
#endif

namespace mega::net {
namespace {

#ifdef MEGA_WITH_EOS

/// Epic Online Services backend used by the PC, Mobile and Pocket editions.
/// The platform handle is ticked from `poll` so the game loop stays
/// single-threaded on every supported device.
class EosNetClient final : public NetClient {
public:
    explicit EosNetClient(std::string lobbyId) : lobbyId_(std::move(lobbyId)) {}

    ~EosNetClient() override { disconnect(); }

    bool connect(const std::string& playerName, const std::string& brawlerId) override {
        playerName_ = playerName;
        brawlerId_ = brawlerId;

        EOS_InitializeOptions init{};
        init.ApiVersion = EOS_INITIALIZE_API_LATEST;
        init.ProductName = "Mega Stars";
        init.ProductVersion = "0.1.0";
        if (EOS_Initialize(&init) != EOS_EResult::EOS_Success) {
            return false;
        }

        EOS_Platform_Options options{};
        options.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
        options.ProductId = std::getenv("MEGA_EOS_PRODUCT_ID");
        options.SandboxId = std::getenv("MEGA_EOS_SANDBOX_ID");
        options.DeploymentId = std::getenv("MEGA_EOS_DEPLOYMENT_ID");
        options.ClientCredentials.ClientId = std::getenv("MEGA_EOS_CLIENT_ID");
        options.ClientCredentials.ClientSecret = std::getenv("MEGA_EOS_CLIENT_SECRET");
        platform_ = EOS_Platform_Create(&options);
        return platform_ != nullptr;
    }

    void sendInput(const PlayerInput&) override {
        // P2P input relay is wired up once the EOS product credentials exist.
    }

    void poll(World&) override {
        if (platform_ != nullptr) {
            EOS_Platform_Tick(platform_);
        }
    }

    void disconnect() override {
        if (platform_ != nullptr) {
            EOS_Platform_Release(platform_);
            platform_ = nullptr;
            EOS_Shutdown();
        }
    }

    bool connected() const override { return platform_ != nullptr; }

    EntityId localPlayerId() const override { return kInvalidEntity; }

private:
    std::string lobbyId_;
    std::string playerName_;
    std::string brawlerId_;
    EOS_HPlatform platform_ = nullptr;
};

#endif  // MEGA_WITH_EOS

}  // namespace

std::unique_ptr<NetClient> makeEosNetClient(const std::string& lobbyId) {
#ifdef MEGA_WITH_EOS
    return std::make_unique<EosNetClient>(lobbyId);
#else
    (void)lobbyId;
    std::fputs(
        "Este binario foi compilado sem o EOS SDK. Recompile com -DMEGA_EOS_SDK=<caminho>.\n",
        stderr);
    return nullptr;
#endif
}

}  // namespace mega::net
