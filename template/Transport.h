
#pragma once

class Transport {
public:
    enum State {
        STOP = 0,
        PLAY = 1,
        RECORD = 2
    };

private:
    State state_ = STOP;

public:
    void Init() {
        state_ = STOP;
    }

    void SetState(State next) {
        state_ = next;
    }

    State GetState() const {
        return state_;
    }

    bool IsRunning() const {
        return state_ != STOP;
    }

    bool IsRecording() const {
        return state_ == RECORD;
    }
};
