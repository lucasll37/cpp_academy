#pragma once

namespace patterns {

struct Command {
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

} // namespace patterns