#include <iostream>
#include <leon/editor/EditorOutputLog.h>
#include <sstream>
#include <streambuf>


namespace leon::editor {
namespace {

class TeeBuf final : public std::streambuf {
public:
    TeeBuf(std::streambuf* primary, EEditorLogLevel level) : primary_(primary), level_(level) {}

protected:
    int overflow(int ch) override {
        if (ch == EOF) {
            return !EOF;
        }
        const int r = primary_ != nullptr ? primary_->sputc(static_cast<char>(ch)) : ch;
        if (ch == '\n') {
            FlushLine();
        } else {
            line_.push_back(static_cast<char>(ch));
        }
        return r;
    }

    int sync() override {
        FlushLine();
        return primary_ != nullptr ? primary_->pubsync() : 0;
    }

private:
    void FlushLine() {
        if (line_.empty()) {
            return;
        }
        EditorOutputLog::Instance().Append(level_, line_);
        line_.clear();
    }

    std::streambuf* primary_ = nullptr;
    EEditorLogLevel level_ = EEditorLogLevel::Info;
    std::string line_;
};

TeeBuf*& CoutTee() {
    static TeeBuf* p = nullptr;
    return p;
}
TeeBuf*& CerrTee() {
    static TeeBuf* p = nullptr;
    return p;
}

} // namespace

EditorOutputLog& EditorOutputLog::Instance() {
    static EditorOutputLog log;
    return log;
}

void EditorOutputLog::Append(EEditorLogLevel level, std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    if (text.empty()) {
        return;
    }
    std::lock_guard lock(mutex_);
    lines_.push_back(EditorLogLine{level, std::move(text)});
    if (lines_.size() > kMaxLines) {
        lines_.erase(lines_.begin(),
                     lines_.begin() + static_cast<std::ptrdiff_t>(lines_.size() - kMaxLines));
    }
}

void EditorOutputLog::Clear() {
    std::lock_guard lock(mutex_);
    lines_.clear();
}

std::vector<EditorLogLine> EditorOutputLog::Snapshot() const {
    std::lock_guard lock(mutex_);
    return lines_;
}

std::size_t EditorOutputLog::Count() const {
    std::lock_guard lock(mutex_);
    return lines_.size();
}

void EditorOutputLog::InstallStreamTee() {
    if (teeInstalled_) {
        return;
    }
    teeInstalled_ = true;
    CoutTee() = new TeeBuf(std::cout.rdbuf(), EEditorLogLevel::Info);
    CerrTee() = new TeeBuf(std::cerr.rdbuf(), EEditorLogLevel::Error);
    std::cout.rdbuf(CoutTee());
    std::cerr.rdbuf(CerrTee());
    Append(EEditorLogLevel::Info, "Output Log ready");
}

} // namespace leon::editor
