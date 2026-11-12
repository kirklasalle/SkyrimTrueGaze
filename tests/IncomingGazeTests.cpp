#include "TestSupport.hpp"
#include "Visuals/IncomingGaze.hpp"
#include "Visuals/IncomingUi.hpp"
#include "Visuals/DiagnosticMenuPolicy.hpp"

#include <cassert>
#include <limits>
#include <iostream>

namespace I = TrueGaze::Visuals::Incoming;
namespace G = TrueGaze::Visuals::Geometry;

I::Receiver Receiver(uint32_t id, float y)
{
    I::Receiver r{};
    r.target = id;
    r.surfaces[0] = {{0, y, 0}, 1, I::Region::Head, I::Quality::Approximate};
    r.surfaceCount = 1;
    r.front = {{0, y - 2, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}, 2, 2, true};
    return r;
}

int main()
{
    I::Frame frame;
    frame.Begin(1);
    auto a = Receiver(1, 0);
    auto b = Receiver(2, 10);
    assert(frame.AddReceiver(a));
    assert(frame.AddReceiver(b));
    assert(frame.AddRay({1, I::Source::PosedEyes, {0, 0, 0}, {0, 1, 0}, 30, 1}));
    frame.Resolve(true, true);
    assert(frame.HitCount() == 2);
    for (size_t i = 0; i < frame.HitCount(); ++i)
    {
        assert(frame.Hits()[i].observer == 1);
        assert(frame.Hits()[i].target == 2);
    }
    const auto anatomy = I::Intersect({1, I::Source::PosedEyes, {0, 0, 0}, {0, 1, 0}, 30, 1},
                                      b, I::Measurement::Anatomy);
    assert(anatomy.hit && std::abs(anatomy.distance - 9) < 0.001f);
    b.front.center = {20, 10, 0};
    assert(!I::Intersect({1, I::Source::PosedEyes, {0, 0, 0}, {0, 1, 0}, 30, 1},
                        b, I::Measurement::FrontPanel).hit);
    assert(I::Intersect({1, I::Source::PosedEyes, {0, 0, 0}, {0, 1, 0}, 30, 1},
                        b, I::Measurement::Anatomy).distance == anatomy.distance);

    frame.Begin(2);
    frame.AddReceiver(Receiver(1, 0));
    frame.AddReceiver(Receiver(2, 10));
    frame.AddRay({1, I::Source::PosedEyes, {0, 0, 0}, {0, 1, 0}, 30, 2});
    frame.AddRay({2, I::Source::PosedEyes, {0, 10, 0}, {0, -1, 0}, 30, 2});
    frame.AddRay({3, I::Source::Camera, {0, -2, 0}, {0, 1, 0}, 30, 2});
    frame.Resolve(true, false);
    assert(frame.HitCount() == 3);
    assert(frame.Hits()[0].target == 2 && frame.Hits()[1].target == 1);
    assert(frame.Hits()[2].source == I::Source::Camera);

    frame.Begin(3);
    assert(frame.HitCount() == 0);
    assert(!frame.AddRay({1, I::Source::Camera, {}, {}, 30, 3}));
    assert(!frame.AddRay({1, I::Source::Camera, {}, {0, 1, 0}, 30, 2}));
    assert(!frame.AddRay({1, I::Source::Camera, {},
                         {std::numeric_limits<float>::quiet_NaN(), 1, 0}, 30, 3}));
    assert(frame.Invalid() == 3);
    for (uint32_t i = 1; i <= I::kActorCapacity; ++i)
        assert(frame.AddReceiver(Receiver(i, static_cast<float>(i * 10))));
    assert(!frame.AddReceiver(Receiver(100, 10)));
    assert(frame.Overflow() == 1);
    frame.AddRay({100, I::Source::Camera, {}, {0, 1, 0}, 1000, 3});
    frame.Resolve(true, true);
    assert(frame.HitCount() == I::kActorCapacity + 1);

    // World-space rotated panel fixture: +X is normal, +Y is its right.
    auto rotated = Receiver(2, 10);
    rotated.front = {{10, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, 3, 2, true};
    const auto quad = I::Intersect({1, I::Source::Camera, {0, 1, 1}, {1, 0, 0}, 30, 4},
                                   rotated, I::Measurement::FrontPanel);
    assert(quad.hit && std::abs(quad.x - 1) < 0.001f && std::abs(quad.z - 1) < 0.001f);
    assert(!I::Intersect({1, I::Source::Camera, {0, 4, 0}, {1, 0, 0}, 30, 4},
                        rotated, I::Measurement::FrontPanel).hit);
    assert(!I::Intersect({1, I::Source::Camera, {}, {-1, 0, 0}, 30, 4},
                        rotated, I::Measurement::FrontPanel).hit);
    assert(I::Intersect({1, I::Source::Camera, {0, 3, 2}, {1, 0, 0}, 10, 4},
                        rotated, I::Measurement::FrontPanel).hit);
    assert(!I::Intersect({1, I::Source::Camera, {}, {0, 1, 0}, 30, 4},
                        rotated, I::Measurement::FrontPanel).hit);
    rotated.front.right = {0, 2, 0};
    assert(!I::Intersect({1, I::Source::Camera, {}, {1, 0, 0}, 30, 4},
                        rotated, I::Measurement::FrontPanel).hit);

    // Retention must not depend on actor traversal order.
    I::Frame reversed;
    frame.Begin(5);
    reversed.Begin(5);
    for (uint32_t i = 1; i <= 70; ++i)
    {
        frame.AddReceiver(Receiver(i, static_cast<float>(i * 10)));
        reversed.AddReceiver(Receiver(71 - i, static_cast<float>((71 - i) * 10)));
        frame.AddRay({i, I::Source::PosedEyes, {}, {0, 1, 0}, 1000, 5});
        reversed.AddRay({71 - i, I::Source::PosedEyes, {}, {0, 1, 0}, 1000, 5});
    }
    frame.AddRay({100, I::Source::Camera, {}, {0, 1, 0}, 1000, 5});
    reversed.AddRay({100, I::Source::Camera, {}, {0, 1, 0}, 1000, 5});
    frame.Resolve(true, true);
    reversed.Resolve(true, true);
    assert(frame.RayCount() == 65 && frame.ReceiverCount() == 64);
    assert(frame.Overflow() == 12 && reversed.Overflow() == 12);
    assert(frame.HitCount() == reversed.HitCount());
    for (size_t i = 0; i < frame.HitCount(); ++i)
    {
        assert(frame.Hits()[i].observer == reversed.Hits()[i].observer);
        assert(frame.Hits()[i].target == reversed.Hits()[i].target);
        assert(frame.Hits()[i].measurement == reversed.Hits()[i].measurement);
    }
    frame.Begin(6);
    frame.AddReceiver(Receiver(3, 20));
    frame.AddReceiver(Receiver(2, 10));
    frame.AddRay({1, I::Source::Camera, {}, {0, 1, 0}, 30, 6});
    frame.Resolve(true, false);
    assert(frame.HitCount() == 1 && frame.Hits()[0].target == 2);
    assert(!frame.Hits()[0].visibilityVerified);

    // Long/right-edge annotations and stacked bottom rows must remain on-screen.
    namespace Ui = TrueGaze::Visuals::IncomingUi;
    const auto rightEdge = Ui::PlaceText(1200, 400, 500, 26);
    assert(rightEdge.x == 768 && rightEdge.x + rightEdge.width == 1268);
    const auto bottomRow = Ui::PlaceText(300, 950, 200, 26);
    assert(bottomRow.y == 682 && bottomRow.y + bottomRow.height == 708);
    const auto oversized = Ui::PlaceText(-200, -200, 2000, 1000);
    assert(oversized.x == 12 && oversized.y == 12);
    assert(oversized.width == 1256 && oversized.height == 696);

    frame.Begin(8);
    frame.AddReceiver(Receiver(2, 10));
    frame.AddRay({1, I::Source::Camera, {0, 0, 0}, {0, 1, 0}, 30, 8});
    frame.AddRay({3, I::Source::PosedEyes, {0, 0, 0}, {0, 1, 0}, 30, 8});
    frame.Resolve(true, true);
    const auto counts = Ui::CountIncoming({frame.Hits().data(), frame.HitCount()});
    assert(counts.at(2).measurements == 4 && counts.at(2).observers.size() == 2);
    assert(Ui::Summary(counts.at(2).observers.size()) == "Gaze matches from 2 observers.");
    frame.Begin(9);
    assert(Ui::CountIncoming({frame.Hits().data(), frame.HitCount()}).empty());
    assert(Ui::Summary(0) == "No incoming gaze matches detected.");
    assert(Ui::Summary(1) == "Gaze matches from 1 observer.");

    auto humanHit = anatomy;
    humanHit.source = I::Source::Camera;
    assert(Ui::Describe(humanHit, "You", "Simmir", false) ==
           "Your view points toward Simmir's head area (estimated).");
    humanHit.source = I::Source::PosedEyes;
    humanHit.region = I::Region::LeftEye;
    assert(Ui::Describe(humanHit, "Lydia", "You", true) ==
           "Lydia's gaze points toward your left eye area (estimated).");
    humanHit.measurement = I::Measurement::FrontPanel;
    assert(Ui::Describe(humanHit, "Lydia", "Simmir", false) ==
           "Lydia's gaze crosses Simmir's front panel.");
    humanHit.measurement = I::Measurement::AbovePanel;
    assert(Ui::Describe(humanHit, "Lydia", "You", true) ==
           "Lydia's gaze crosses your above-head panel.");
    std::map<uint32_t, std::string> actorNames{{2, "Guard"}, {3, "Guard"}, {4, "Simmir"}};
    Ui::IdentifyNames(actorNames);
    assert(actorNames.at(2) == "Guard [00000002]" && actorNames.at(3) == "Guard [00000003]");
    assert(actorNames.at(4) == "Simmir");

    const std::vector<Ui::TextRect> protectedAreas{{500, 290, 280, 280}, {390, 240, 80, 96}};
    std::vector<Ui::CardRequest> cardRequests{
        {2, 420, 300, 320, 180, true}, {3, 425, 305, 320, 110, true},
        {4, 1200, 600, 320, 90, false}, {5, 300, 300, 320, 800, true}};
    const auto placed = Ui::LayoutCards(cardRequests, protectedAreas, {});
    assert(placed.placed.size() == 3 && placed.overflow == std::vector<uint32_t>{5});
    std::map<uint32_t, Ui::PlacedCard> prior;
    for (const auto& card : placed.placed)
    {
        assert(card.rect.x >= 12 && card.rect.y >= 156);
        assert(card.rect.x + card.rect.width <= 1268 && card.rect.y + card.rect.height <= 608);
        for (const auto& area : protectedAreas)
            assert(!Ui::Overlaps(card.rect, area));
        for (const auto& other : placed.placed)
            if (other.id != card.id)
                assert(!Ui::Overlaps(card.rect, other.rect));
        prior.emplace(card.id, card);
    }
    const auto stable = Ui::LayoutCards(cardRequests, protectedAreas, prior);
    for (const auto& card : stable.placed)
        assert(card.rect.x == prior.at(card.id).rect.x && card.rect.y == prior.at(card.id).rect.y);
    const std::vector<Ui::CardRequest> cameraBefore{{10, 640, 200, 320, 80, true}};
    const auto beforeMove = Ui::LayoutCards(cameraBefore, {}, {});
    assert(beforeMove.placed.size() == 1);
    const std::map<uint32_t, Ui::PlacedCard> beforePosition{{10, beforeMove.placed.front()}};
    const std::vector<Ui::CardRequest> cameraAfter{{10, 642, 202, 320, 80, true}};
    const auto afterMove = Ui::LayoutCards(cameraAfter, {}, beforePosition);
    assert(afterMove.placed.front().rect.x == beforeMove.placed.front().rect.x + 2 &&
           afterMove.placed.front().rect.y == beforeMove.placed.front().rect.y + 2);
    std::reverse(cardRequests.begin(), cardRequests.end());
    const auto deterministic = Ui::LayoutCards(cardRequests, protectedAreas, {});
    assert(deterministic.placed.size() == placed.placed.size());
    for (size_t i = 0; i < placed.placed.size(); ++i)
        assert(deterministic.placed[i].id == placed.placed[i].id &&
               deterministic.placed[i].rect.x == placed.placed[i].rect.x &&
               deterministic.placed[i].rect.y == placed.placed[i].rect.y);
    const std::vector<Ui::TextRect> blocked{{12, 156, 1256, 452}};
    const auto overflow = Ui::LayoutCards(cardRequests, blocked, {});
    assert(overflow.placed.empty() && overflow.overflow.size() == cardRequests.size());
    std::vector<Ui::CardRequest> crowd;
    for (uint32_t id = 1; id <= 64; ++id)
        crowd.push_back({id, 640, 400, 320, 100, id == 64});
    const auto crowded = Ui::LayoutCards(crowd, protectedAreas, {});
    assert(!crowded.overflow.empty() && crowded.placed.front().id == 64);
    assert(crowded.placed.size() + crowded.overflow.size() == crowd.size());
    for (const auto& card : crowded.placed)
        for (const auto& other : crowded.placed)
            if (card.id != other.id)
                assert(!Ui::Overlaps(card.rect, other.rect));

    auto page = Ui::StepInspection({0, 1}, 12, 5, 3, true);
    assert(page.entry == 0 && page.scroll == 6);
    page = Ui::StepInspection({0, 12}, 12, 5, 3, true);
    assert(page.entry == 1 && page.scroll == 1);
    page = Ui::StepInspection(page, 1, 5, 3, false);
    assert(page.entry == 0 && page.scroll == std::numeric_limits<int>::max());
    page = Ui::StepInspection({2, 1}, 1, 5, 3, true);
    assert(page.entry == 0 && page.scroll == 1);
    Ui::TimingWindow timings;
    assert(timings.Read().samples == 0);
    for (uint64_t value = 1; value <= 300; ++value)
        timings.Add(value);
    const auto rolling = timings.Read();
    assert(rolling.samples == 256 && rolling.median == 173 && rolling.p95 == 288 && rolling.peak == 300);

    TrueGaze::Visuals::DiagnosticMenuPolicy menuPolicy;
    const auto gameplay = menuPolicy.Snapshot();
    assert(menuPolicy.Allows(gameplay));
    assert(!menuPolicy.SetMenu("Console", true) && menuPolicy.Allows(gameplay));
    assert(menuPolicy.SetMenu("TweenMenu", true) && !menuPolicy.Allows(gameplay));
    assert(!menuPolicy.SetMenu("TweenMenu", true));
    assert(menuPolicy.SetMenu("InventoryMenu", true));
    assert(menuPolicy.SetMenu("TweenMenu", false));
    assert(TrueGaze::Visuals::DiagnosticMenuPolicy::Blocked(menuPolicy.Snapshot()));
    assert(!menuPolicy.Allows(gameplay));
    assert(menuPolicy.SetMenu("InventoryMenu", false));
    assert(!TrueGaze::Visuals::DiagnosticMenuPolicy::Blocked(menuPolicy.Snapshot()));
    assert(!menuPolicy.Allows(gameplay));
    const auto resumedFrame = menuPolicy.Snapshot();
    assert(menuPolicy.Allows(resumedFrame));
    assert(!menuPolicy.SetMenu("InventoryMenu", false) && menuPolicy.Allows(resumedFrame));
    assert(!menuPolicy.SetMenu("Dialogue Menu", true) && menuPolicy.Allows(resumedFrame));
    assert(menuPolicy.SetMenu("MapMenu", true) && !menuPolicy.Allows(resumedFrame));
    assert(menuPolicy.SetMenu("MapMenu", false) && !menuPolicy.Allows(resumedFrame));
    std::cout << "Incoming ownership, reciprocity, multiple sources, panel independence, "
                 "frame cleanup, invalid data, capacity and rotated bounds: PASS\n";
}
