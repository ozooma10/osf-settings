#include "API/DiagnosticsApi.h"
#include "Diagnostics/DiagnosticsService.h"
#include "Input/KeyNames.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <type_traits>

extern "C" void* OSFSettings_RequestDiagnosticsAPI(std::uint32_t, std::uint32_t*) noexcept;

namespace OSFSettings
{
    bool IsBindableKey(std::uint32_t)
    {
        throw std::runtime_error("unexpected native key validation during diagnostics API test");
    }
}

int main()
{
    using namespace OSFSettings;
    using API::Status;
    namespace D = API::Diagnostics;
    static_assert(std::is_abstract_v<D::IDiagnostics> && !std::is_destructible_v<D::IDiagnostics>);
    static_assert(std::is_standard_layout_v<D::Issue> && std::is_trivially_copyable_v<D::Issue>);
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    try {
        DiagnosticsService service;
        API::DiagnosticsApi adapter(service);
        D::Client client;
        const D::Issue issue{
            .modId = "sample", .id = "missing-pack", .severity = D::Severity::Warning,
            .title = "Custom animations are unavailable", .impact = "Scenes use standard animations.",
            .nextSteps = "Install the animation pack and restart."
        };
        check(!client && client.Version() == 0 && !client.Raw(), "new clients are disconnected");
        check(client.Report(issue) == Status::NotReady && client.Clear("sample", "missing-pack") == Status::NotReady &&
            client.ClearMod("sample") == Status::NotReady, "disconnected calls return NotReady");

        std::uint32_t version = 42;
        check(!D::RequestInterface(D::kBaseVersion, &version) && version == 0, "lookup handles an absent plugin without loading it");
        check(!client.Init() && !client, "client initialization fails cleanly without the plugin DLL");
        check(client.Attach(&adapter) && client.Version() == D::kVersion && client.Raw() == &adapter &&
            client.Has(D::kBaseVersion) && !client.Has(D::kVersion + 1), "client attaches a compatible provider");
        check(!client.Attach(&adapter, 0x00020000u) && !client && client.Version() == 0 && !client.Raw(),
            "an incompatible major detaches the client");
        check(client.Attach(&adapter, D::kVersion + 1) && client.Version() == D::kVersion + 1,
            "a provider with a newer compatible minor can be attached");
        check(!client.Attach(nullptr) && !client && client.Version() == 0, "attaching null detaches the client");
        check(client.Attach(&adapter) && client.Report(issue) == Status::Ok, "native reports need no schema or settings initialization");
        check(service.Snapshot().size() == 1 && service.Snapshot()[0].severity == IssueSeverity::Warning,
            "the API maps warning severity into internal storage");

        std::array<std::string, 5> input{ issue.modId, issue.id, issue.title, issue.impact, issue.nextSteps };
        const auto original = input;
        const D::Issue borrowed{
            .modId = input[0].c_str(), .id = input[1].c_str(), .title = input[2].c_str(),
            .impact = input[3].c_str(), .nextSteps = input[4].c_str()
        };
        check(client.Report(borrowed) == Status::Ok, "report accepts caller-owned buffers");
        for (auto& text : input) text.assign("Reused buffer");
        const auto owned = service.Snapshot();
        check(owned.size() == 1 && owned[0].modId == original[0] && owned[0].id == original[1] &&
            owned[0].title == original[2] && owned[0].impact == original[3] && owned[0].nextSteps == original[4],
            "all report strings are copied before the API returns");

        auto other = issue;
        other.modId = "other";
        check(client.Report(other) == Status::Ok && service.Snapshot().size() == 2, "mod IDs isolate reports with the same issue ID");
        auto updated = issue;
        updated.severity = D::Severity::Error;
        updated.title = "Animation loading failed";
        updated.impact = "Scenes cannot start.";
        updated.nextSteps = "Reinstall the animation pack.\nRestart the game.";
        check(client.Report(updated) == Status::Ok && client.Report(updated) == Status::Ok,
            "repeat and changed reports both succeed");
        const auto changed = service.Snapshot();
        check(changed.size() == 2 && changed[0].modId == "sample" && changed[0].severity == IssueSeverity::Error &&
            changed[0].title == updated.title && changed[0].impact == updated.impact && changed[0].nextSteps == updated.nextSteps,
            "updates replace the full report without duplicating it");

        const D::Issue minimal{ .modId = "sample", .id = "missing-pack", .title = "Custom animations are unavailable" };
        check(client.Report(minimal) == Status::Ok && service.Snapshot()[0].impact.empty() && service.Snapshot()[0].nextSteps.empty(),
            "omitted optional pointers are accepted and clear previous optional text");
        for (auto field : { &D::Issue::impact, &D::Issue::nextSteps }) {
            for (const auto* text : std::array<const char*, 3>{ nullptr, "", " \t\r\n" }) {
                auto optional = updated;
                optional.*field = text;
                check(client.Report(optional) == Status::Ok, "null, empty and blank optional fields are accepted");
                const auto stored = service.Snapshot();
                check(stored.size() == 2 && stored[0].impact == (optional.impact ? optional.impact : "") &&
                    stored[0].nextSteps == (optional.nextSteps ? optional.nextSteps : ""),
                    "optional fields are independently copied, with null treated as empty");
            }
        }
        check(client.Report(updated) == Status::Ok, "a later report can restore optional text");

        for (auto field : { &D::Issue::modId, &D::Issue::id, &D::Issue::title }) {
            for (const auto* text : std::array<const char*, 3>{ nullptr, "", " \t\r\n" }) {
                auto invalid = updated;
                invalid.*field = text;
                check(client.Report(invalid) == Status::InvalidArgument, "null or blank required fields are rejected at the native boundary");
            }
        }
        for (const auto* modId : { "MixedCase", "path/mod", ".", ".." }) {
            auto invalid = updated;
            invalid.modId = modId;
            check(client.Report(invalid) == Status::InvalidArgument, "report mod IDs follow the existing settings rules");
        }
        const auto unchanged = service.Snapshot();
        check(unchanged.size() == 2 && unchanged[0].severity == IssueSeverity::Error && unchanged[0].title == updated.title &&
            unchanged[0].impact == updated.impact && unchanged[0].nextSteps == updated.nextSteps && unchanged[1].modId == "other",
            "invalid reports preserve existing issues");

        check(client.Clear(nullptr, "missing-pack") == Status::InvalidArgument && client.Clear("sample", nullptr) == Status::InvalidArgument &&
            client.Clear("MixedCase", "missing-pack") == Status::InvalidArgument && client.Clear("sample", " \t") == Status::InvalidArgument,
            "invalid clear identities are rejected");
        check(client.ClearMod(nullptr) == Status::InvalidArgument && client.ClearMod("") == Status::InvalidArgument &&
            client.ClearMod("MixedCase") == Status::InvalidArgument && service.Snapshot().size() == 2,
            "invalid clear-mod calls preserve existing issues");
        check(client.Clear("unknown", "missing-pack") == Status::Ok && client.Clear("sample", "unknown") == Status::Ok &&
            client.ClearMod("unknown") == Status::Ok && service.Snapshot().size() == 2, "valid clears of absent issues succeed");
        check(client.Clear("sample", "missing-pack") == Status::Ok && client.Clear("sample", "missing-pack") == Status::Ok &&
            service.Snapshot().size() == 1 && service.Snapshot()[0].modId == "other", "clear removes only the named mod's issue");
        auto second = issue;
        second.id = "second";
        check(client.Report(issue) == Status::Ok && client.Report(second) == Status::Ok && client.ClearMod("sample") == Status::Ok &&
            service.Snapshot().size() == 1 && service.Snapshot()[0].modId == "other", "clear-mod removes all that mod's issues");
        check(client.ClearMod("other") == Status::Ok && service.Snapshot().empty(), "clearing the final mod removes all active issues");

        for (const auto requested : { 0u, 0x00010001u, 0x00020000u }) {
            version = 42;
            check(!OSFSettings_RequestDiagnosticsAPI(requested, &version) && version == 0, "export rejects incompatible ABI versions");
        }
        auto* exported = static_cast<D::IDiagnostics*>(OSFSettings_RequestDiagnosticsAPI(D::kBaseVersion, &version));
        check(exported && version == D::kVersion && OSFSettings_RequestDiagnosticsAPI(D::kBaseVersion, nullptr) == exported,
            "export returns a stable interface and reports its ABI version");
        auto exportedIssue = issue;
        exportedIssue.modId = "export-probe";
        check(exported->Report(exportedIssue) == Status::Ok && DiagnosticsService::Get().Snapshot().size() == 1 &&
            DiagnosticsService::Get().Snapshot()[0].modId == "export-probe", "exported interface writes to the process service");
        check(exported->ClearMod("export-probe") == Status::Ok && DiagnosticsService::Get().Snapshot().empty(),
            "exported interface clears the process service");

        std::cout << checks << '/' << checks << " diagnostics API checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Diagnostics API test failed: " << error.what() << '\n';
        return 1;
    }
}
