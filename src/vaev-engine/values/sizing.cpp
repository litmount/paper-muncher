module;

#include <karm/macros>

export module Vaev.Engine:values.sizing;

import Karm.Core;

import :css;
import :values.base;
import :values.calc;
import :values.keywords;
import :values.length;
import :values.percent;
import :values.writing;

using namespace Karm;

namespace Vaev {

// https://www.w3.org/TR/css-sizing-3/#box-sizing
export enum struct BoxSizing : u8 {
    CONTENT_BOX,
    BORDER_BOX,
};

// https://www.w3.org/TR/css-images-4/#the-object-fit
export enum struct ObjectFit : u8 {
    FILL,
    CONTAIN,
    COVER,
    NONE,
    SCALE_DOWN,

    _LEN
};

export template <>
struct ValueParser<ObjectFit> {
    static Res<ObjectFit> parse(Cursor<Css::Sst>& c) {
        if (c.ended())
            return Error::invalidData("unexpected end of input");

        if (c.skip(Css::Token::ident("fill")))
            return Ok(ObjectFit::FILL);
        else if (c.skip(Css::Token::ident("contain")))
            return Ok(ObjectFit::CONTAIN);
        else if (c.skip(Css::Token::ident("cover")))
            return Ok(ObjectFit::COVER);
        else if (c.skip(Css::Token::ident("none")))
            return Ok(ObjectFit::NONE);
        else if (c.skip(Css::Token::ident("scale-down")))
            return Ok(ObjectFit::SCALE_DOWN);
        else
            return Error::invalidData("expected object-fit value");
    }
};

// MARK: FitContent
// https://drafts.csswg.org/css-sizing-3/#preferred-size-properties

export struct FitContent {
    CalcValue<PercentOr<Length>> value = {Length{}};

    void repr(Io::Emit& e) const {
        e("(fit-content {})", value);
    }
};

export template <>
struct ValueParser<FitContent> {
    static Res<FitContent> parse(Cursor<Css::Sst>& c) {
        if (c.ended())
            return Error::invalidData("unexpected end of input");

        if (c->prefix == Css::Token::function("fit-content(")) {
            FitContent result;
            Cursor<Css::Sst> scan = c->content;
            result.value = try$(parseValue<PercentOr<Length>>(scan));
            c.next();
            return Ok(result);
        }
        return Error::invalidData("invalid fit-content");
    }
};

// https://www.w3.org/TR/css-sizing-3/#propdef-width
// https://www.w3.org/TR/css-sizing-3/#propdef-height
export using Size = FlatUnion<Keywords::Auto, CalcValue<PercentOr<Length>>, Keywords::MinContent, Keywords::MaxContent, FitContent>;
export using MaxSize = FlatUnion<Keywords::None, CalcValue<PercentOr<Length>>, Keywords::MinContent, Keywords::MaxContent, FitContent>;

export struct SizingProps {
    Size width = Keywords::AUTO, height = Keywords::AUTO;
    Size minWidth = Keywords::AUTO, minHeight = Keywords::AUTO;
    MaxSize maxWidth = Keywords::NONE, maxHeight = Keywords::NONE;
    ObjectFit objectFit = ObjectFit::FILL;

    Size& size(Axis axis) {
        return axis == Axis::HORIZONTAL ? width : height;
    }

    Size const size(Axis axis) const {
        return axis == Axis::HORIZONTAL ? width : height;
    }

    Size& minSize(Axis axis) {
        return axis == Axis::HORIZONTAL ? minWidth : minHeight;
    }

    Size const minSize(Axis axis) const {
        return axis == Axis::HORIZONTAL ? minWidth : minHeight;
    }

    MaxSize& maxSize(Axis axis) {
        return axis == Axis::HORIZONTAL ? maxWidth : maxHeight;
    }

    MaxSize const maxSize(Axis axis) const {
        return axis == Axis::HORIZONTAL ? maxWidth : maxHeight;
    }

    void repr(Io::Emit& e) const {
        e("(sizing");
        e(" width={}", width);
        e(" height={}", height);
        e(" minWidth={}", minWidth);
        e(" minHeight={}", minHeight);
        e(" maxWidth={}", maxWidth);
        e(" maxHeight={}", maxHeight);
        e(" objectFit={}", objectFit);
        e(")");
    }
};

} // namespace Vaev
