#pragma once
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <charconv>

namespace partprac {
struct Range {
    float startX=0, startY=0, endX=0, endY=0;
    bool valid() const {
        return std::isfinite(startX)&&std::isfinite(startY)&&std::isfinite(endX)&&std::isfinite(endY)
            &&startX>=0&&endX-startX>=30&&startY>=0;
    }
};
inline std::optional<std::string_view> objectProperty(std::string_view object,std::string_view key) {
    if(object.ends_with(';')) object.remove_suffix(1);
    for(size_t p=0;p<object.size();) {
        auto comma=object.find(',',p);
        if(comma==std::string_view::npos) return {};
        auto end=object.find(',',comma+1);
        if(end==std::string_view::npos) end=object.size();
        if(object.substr(p,comma-p)==key) return object.substr(comma+1,end-comma-1);
        p=end+1;
    }
    return {};
}
// GD serializes StartPosObject positions through its own save routine. On GD 2.2081,
// the encoded positions can differ from the editor drag coordinates, so comparing
// them against the exact original floating-point values can reject a valid marker.
// The selected range was already verified before constructing the object. Validate
// the native marker structure and numeric coordinates, then let PlayLayer's native
// Start Pos activation guard reject a marker that the game cannot actually use.
inline bool validStartPosition(std::string_view object,Range const& range) {
    if(!range.valid()||objectProperty(object,"1")!="31"
        ||objectProperty(object,"kA21")=="1") return false;
    auto encodedCoordinate=[&](std::string_view key) {
        auto value=objectProperty(object,key);
        if(!value) return false;
        float parsed=0;
        auto result=std::from_chars(value->data(),value->data()+value->size(),parsed);
        return result.ec==std::errc{} && result.ptr==value->data()+value->size()
            && std::isfinite(parsed) && parsed>=0;
    };
    return encodedCoordinate("2")&&encodedCoordinate("3");
}
// Preserve every original object and property verbatim except existing Start Pos objects.
// The level header is retained, including song offset, colors and gameplay settings.
inline std::string withoutStartPositions(std::string_view level) {
    std::string result;
    bool header=true;
    for(size_t begin=0;begin<level.size();) {
        auto end=level.find(';',begin);
        if(end==std::string_view::npos) end=level.size();
        auto object=level.substr(begin,end-begin);
        bool startPos=false;
        if(!header) {
            for(size_t p=0;p<object.size();) {
                auto comma=object.find(',',p);
                if(comma==std::string_view::npos) break;
                auto valueEnd=object.find(',',comma+1);
                if(valueEnd==std::string_view::npos) valueEnd=object.size();
                if(object.substr(p,comma-p)=="1"&&object.substr(comma+1,valueEnd-comma-1)=="31") startPos=true;
                p=valueEnd+1;
            }
        }
        if(!startPos&&!object.empty()) { result.append(object); result+=';'; }
        header=false; begin=end+1;
    }
    return result;
}
}
