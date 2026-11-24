#include "game/DemoWorld.hpp"

#include "game/App.hpp"

namespace hr {

void DemoWorld::update(float dt)
{
    if (!session_ || session_->isOver() || session_->time() > 70.0f) {
        session_ = std::make_unique<Session>(stages().front(), vehicles().front(), kStockLevels, 0.0f);
        view_.reset(session_->stage(), session_->vehicle().position());
        bot_ = Bot();
        particles_.clear();
        pose_ = poseOf(*session_);
        prev_ = pose_;
    }
    accumulator_ += dt;
    while (accumulator_ >= kFixedDt) {
        accumulator_ -= kFixedDt;
        prev_ = pose_;
        session_->step(bot_.think(*session_));
        session_->events().clear();
        pose_ = poseOf(*session_);
    }
    const VehiclePose pose = lerpPose(prev_, pose_, accumulator_ / kFixedDt);
    view_.update(dt, pose.chassis.p, session_->vehicle().velocity());
    particles_.update(dt);
}

void DemoWorld::draw(App& app, float time) const
{
    if (!session_) return;
    view_.draw(*session_, lerpPose(prev_, pose_, accumulator_ / kFixedDt), particles_, app.assets(), time, false);
}

} // namespace hr
