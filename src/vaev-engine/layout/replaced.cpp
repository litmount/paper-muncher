export module Vaev.Engine:layout.replaced;

import Karm.Image;
import Karm.Gfx;
import Karm.Math;
import Karm.Logger;
import Karm.Scene;
import Karm.Core;

import :values;
import :layout.base;
import :layout.layout;

namespace Vaev::Layout {

struct ReplacedFormatingContext : FormatingContext {
    static Opt<f64> _intrinsicAspectRatio(Vec2Au intrinsicSize) {
        if (intrinsicSize.y == 0_au)
            return NONE;
        return intrinsicSize.x.cast<f64>() / intrinsicSize.y.cast<f64>();
    }

    Au _resolveMinSize(Tree& tree, Box& box, Size const& size, Au relative, Au intrinsicSize) {
        return size.visit(Visitor{
            [&](CalcValue<PercentOr<Length>> const& calc) {
                return resolve(tree, box, calc, relative);
            },
            [&](Keywords::MinContent const&) {
                return intrinsicSize;
            },
            [&](Keywords::MaxContent const&) {
                return intrinsicSize;
            },
            [&](FitContent const& fit) {
                return min(intrinsicSize, resolve(tree, box, fit.value, relative));
            },
            [&](Keywords::Auto const&) {
                return 0_au;
            },
        });
    }

    Au _resolveMaxSize(Tree& tree, Box& box, MaxSize const& size, Au relative, Au intrinsicSize) {
        return size.visit(Visitor{
            [&](CalcValue<PercentOr<Length>> const& calc) {
                return resolve(tree, box, calc, relative);
            },
            [&](Keywords::MinContent const&) {
                return intrinsicSize;
            },
            [&](Keywords::MaxContent const&) {
                return intrinsicSize;
            },
            [&](FitContent const& fit) {
                return min(intrinsicSize, resolve(tree, box, fit.value, relative));
            },
            [&](Keywords::None const&) {
                return Limits<Au>::MAX;
            },
        });
    }

    Vec2Au _clampByMinMax(Tree& tree, Box& box, Input const& input, Vec2Au size, Vec2Au intrinsicSize, Opt<f64> aspectRatio) {
        auto minWidth = _resolveMinSize(
            tree, box,
            box.style->sizing->minWidth,
            input.containingBlock.x,
            intrinsicSize.x
        );
        auto maxWidth = _resolveMaxSize(
            tree, box,
            box.style->sizing->maxWidth,
            input.containingBlock.x,
            intrinsicSize.x
        );
        auto minHeight = _resolveMinSize(
            tree, box,
            box.style->sizing->minHeight,
            input.containingBlock.y,
            intrinsicSize.y
        );
        auto maxHeight = _resolveMaxSize(
            tree, box,
            box.style->sizing->maxHeight,
            input.containingBlock.y,
            intrinsicSize.y
        );

        size.x = clamp(size.x, minWidth, maxWidth);
        size.y = clamp(size.y, minHeight, maxHeight);

        if (aspectRatio and input.knownSize.x and not input.knownSize.y) {
            size.y = clamp(size.x / *aspectRatio, minHeight, maxHeight);
        } else if (aspectRatio and input.knownSize.y and not input.knownSize.x) {
            size.x = clamp(size.y * *aspectRatio, minWidth, maxWidth);
        }

        return size;
    }

    Vec2Au updateRelativeTo(SvgRootBox& root, SvgRootFrag& svgFrag) {
        // https://svgwg.org/svg2-draft/coords.html#Units
        // SPEC: For <percentage> values that are defined to be relative to the size of SVG viewport:
        // the value to use must be the percentage, in user units, of the * parameter of the ‘viewBox’ applied to that
        // viewport. If no ‘viewBox’ is specified, then the value to use must be the percentage, in user units, of the
        // * of the SVG viewport.
        return root.viewBox
                   ? Vec2Au{Au{root.viewBox->width}, Au{root.viewBox->height}}
                   : svgFrag
                         .transf.inverse()
                         .applyVector(
                             Vec2Au{
                                 svgFrag.boundingBox.width,
                                 svgFrag.boundingBox.height,
                             }
                                 .cast<f64>()
                         )
                         .cast<Au>();
    }

    SvgGroupFrag::Element buildSVGFrag(Tree& tree, SvgGroupBox::Element& element, Vec2Au resolveTo) {
        if (auto shape = element.is<SvgShapeBox>()) {
            return SvgShapeFrag::fromShape(*shape, resolveTo);
        } else if (auto nestedGroup = element.is<SvgGroupBox>()) {
            SvgGroupFrag nestedGroupFrag{&(*nestedGroup)};
            buildSVGAggregateFrag(tree, *nestedGroup, nestedGroupFrag, resolveTo);
            return nestedGroupFrag;
        } else if (auto nestedRoot = element.is<SvgRootBox>()) {
            auto resolvedRect = resolve(buildRectangle(*nestedRoot->style), resolveTo);

            return buildSVGRootFrag(
                tree, *nestedRoot,
                {resolvedRect.x, resolvedRect.y},
                {resolvedRect.width, resolvedRect.height}
            );
        } else if (auto box = element.is<::Box<Box>>()) {
            auto resolvedRect = resolve(buildRectangle(*(*box)->style), resolveTo);

            auto frag = Frag();
            Input childInput{
                .knownSize = {resolvedRect.width, resolvedRect.height},
                .position = {resolvedRect.x, resolvedRect.y},
            };

            auto output = layoutAndCommitBorderBox(tree, **box, childInput, frag, UsedSpacings{});
            return makeBox<Frag>(std::move(frag.children()[0]));
        }
        unreachable();
    }

    void buildSVGAggregateFrag(Tree& tree, SvgGroupBox& group, SvgGroupFrag& groupFrag, Vec2Au resolveTo) {
        for (auto& element : group.elements) {
            groupFrag.add(buildSVGFrag(tree, element, resolveTo));
        }
    }

    SvgRootFrag buildSVGRootFrag(Tree& tree, SvgRootBox& root, Vec2Au position, Vec2Au size) {
        SvgRootFrag svgFrag = SvgRootFrag::build(root, position, size);
        buildSVGAggregateFrag(tree, root, svgFrag, updateRelativeTo(root, svgFrag));
        return svgFrag;
    }

    // https://www.w3.org/TR/css-images-3/#default-sizing
    // NOTE: not fully compliant
    Vec2Au _defaultSizing(Math::Vec2<Opt<Au>> knownSize, Opt<f64> aspectRatio, Vec2Au defaultSize) {
        if (knownSize.x and knownSize.y)
            return {*knownSize.x, *knownSize.y};

        Vec2Au size = defaultSize;
        if (knownSize.x) {
            size.x = *knownSize.x;
            if (aspectRatio)
                size.y = size.x / *aspectRatio;
        } else if (knownSize.y) {
            size.y = *knownSize.y;
            if (aspectRatio)
                size.x = size.y * *aspectRatio;
        } else if (aspectRatio) {
            size.y = size.x / *aspectRatio;
        }
        return size;
    }

    Output run(Tree& tree, Box& box, Input input, [[maybe_unused]] usize startAt, [[maybe_unused]] Opt<usize> stopAt) override {
        Vec2Au size = {};

        if (auto image = box.content.is<Rc<Scene::Node>>()) {
            auto intrinsicSize = (*image)->bound().size().cast<Au>();
            auto aspectRatio = _intrinsicAspectRatio(intrinsicSize);

            size = _defaultSizing(input.knownSize, aspectRatio, intrinsicSize);
            size = _clampByMinMax(tree, box, input, size, intrinsicSize, aspectRatio);
        } else if (auto svg = box.content.is<SvgRootBox>()) {
            auto aspectRatio = intrinsicAspectRatio(box.style->svg->viewBox, box.style->sizing->width, box.style->sizing->height);

            size = _defaultSizing(input.knownSize, aspectRatio, input.containingBlock);

            if (input.fragment) {
                auto svgFrag = buildSVGRootFrag(tree, *svg, input.position, size);
                svgFrag.computeBoundingBoxes();
                input.fragment->content = svgFrag;
            }
        } else {
            panic("unsupported replaced content");
        }

        if (tree.fc.allowBreak() and
            not tree.fc.acceptsFit(
                input.position.y,
                size.y,
                input.pendingVerticalSizes
            )) {
            return {
                .size = {},
                .completelyLaidOut = false,
                .breakpoint = Breakpoint::overflow(),
                .firstBaselineSet = BaselinePositionsSet::fromSinglePosition(size.y),
                .lastBaselineSet = BaselinePositionsSet::fromSinglePosition(size.y),
            };
        }

        return {
            .size = size,
            .completelyLaidOut = true,
            .firstBaselineSet = BaselinePositionsSet::fromSinglePosition(size.y),
            .lastBaselineSet = BaselinePositionsSet::fromSinglePosition(size.y),
        };
    }
};

export Rc<FormatingContext> constructReplacedFormatingContext(Box&) {
    return makeRc<ReplacedFormatingContext>();
}

} // namespace Vaev::Layout
