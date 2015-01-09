/*
    Copyright (C) 2005 Apple Inc.
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2008 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>

    Based on khtml css code by:
    Copyright(C) 1999-2003 Lars Knoll(knoll@kde.org)
             (C) 2003 Apple Inc.
             (C) 2004 Allan Sandfeld Jensen(kde@carewolf.com)
             (C) 2004 Germain Garand(germain@ebooksfrance.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "StyleResolver.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSShadowValue.h"
#include "CSSValueList.h"
#include "Document.h"
#include "SVGColor.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGPaint.h"
#include "SVGRenderStyle.h"
#include "SVGRenderStyleDefs.h"
#include "SVGURIReference.h"
#include <stdlib.h>
#include <wtf/MathExtras.h>

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) \
{ \
    svgStyle.set##Prop(state.parentStyle()->svgStyle().prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    svgStyle.set##Prop(SVGRenderStyle::initial##Prop()); \
    return; \
}

namespace WebCore {

static float roundToNearestGlyphOrientationAngle(float angle)
{
    angle = fabsf(fmodf(angle, 360.0f));

    if (angle <= 45.0f || angle > 315.0f)
        return 0.0f;
    else if (angle > 45.0f && angle <= 135.0f)
        return 90.0f;
    else if (angle > 135.0f && angle <= 225.0f)
        return 180.0f;

    return 270.0f;
}

static int angleToGlyphOrientation(float angle)
{
    angle = roundToNearestGlyphOrientationAngle(angle);

    if (angle == 0.0f)
        return GO_0DEG;
    else if (angle == 90.0f)
        return GO_90DEG;
    else if (angle == 180.0f)
        return GO_180DEG;
    else if (angle == 270.0f)
        return GO_270DEG;

    return -1;
}

static Color colorFromSVGColorCSSValue(SVGColor* svgColor, const Color& fgColor)
{
    Color color;
    if (svgColor->colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR)
        color = fgColor;
    else
        color = svgColor->color();
    return color;
}

// FIXME: This method should go away once all SVG CSS properties have been
// ported to the new generated StyleBuilder.
void StyleResolver::applySVGProperty(CSSPropertyID id, CSSValue* value)
{
    ASSERT(value);
    CSSPrimitiveValue* primitiveValue = nullptr;
    if (is<CSSPrimitiveValue>(*value))
        primitiveValue = downcast<CSSPrimitiveValue>(value);

    const State& state = m_state;
    SVGRenderStyle& svgStyle = state.style()->accessSVGStyle();

    bool isInherit = state.parentStyle() && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!state.parentStyle() && value->isInheritedValue());

    // What follows is a list that maps the CSS properties into their
    // corresponding front-end RenderStyle values. Shorthands(e.g. border,
    // background) occur in this list as well and are only hit when mapping
    // "inherit" or "initial" into front-end values.
    switch (id)
    {
        // ident only properties
        case CSSPropertyBaselineShift:
        {
            HANDLE_INHERIT_AND_INITIAL(baselineShift, BaselineShift);
            if (!primitiveValue)
                break;

            if (primitiveValue->getValueID()) {
                switch (primitiveValue->getValueID()) {
                case CSSValueBaseline:
                    svgStyle.setBaselineShift(BS_BASELINE);
                    break;
                case CSSValueSub:
                    svgStyle.setBaselineShift(BS_SUB);
                    break;
                case CSSValueSuper:
                    svgStyle.setBaselineShift(BS_SUPER);
                    break;
                default:
                    break;
                }
            } else {
                svgStyle.setBaselineShift(BS_LENGTH);
                svgStyle.setBaselineShiftValue(SVGLength::fromCSSPrimitiveValue(primitiveValue));
            }

            break;
        }
        case CSSPropertyKerning:
        {
            HANDLE_INHERIT_AND_INITIAL(kerning, Kerning);
            if (primitiveValue)
                svgStyle.setKerning(SVGLength::fromCSSPrimitiveValue(primitiveValue));
            break;
        }
        case CSSPropertyColorProfile:
        {
            // Not implemented.
            break;
        }
        case CSSPropertyPaintOrder: {
            HANDLE_INHERIT_AND_INITIAL(paintOrder, PaintOrder)
            // 'normal' is the only primitiveValue
            if (primitiveValue)
                svgStyle.setPaintOrder(PaintOrderNormal);
            if (!is<CSSValueList>(*value))
                break;
            CSSValueList& orderTypeList = downcast<CSSValueList>(*value);

            // Serialization happened during parsing. No additional checking needed.
            unsigned length = orderTypeList.length();
            primitiveValue = downcast<CSSPrimitiveValue>(orderTypeList.itemWithoutBoundsCheck(0));
            PaintOrder paintOrder;
            switch (primitiveValue->getValueID()) {
            case CSSValueFill:
                paintOrder = length > 1 ? PaintOrderFillMarkers : PaintOrderFill;
                break;
            case CSSValueStroke:
                paintOrder = length > 1 ? PaintOrderStrokeMarkers : PaintOrderStroke;
                break;
            case CSSValueMarkers:
                paintOrder = length > 1 ? PaintOrderMarkersStroke : PaintOrderMarkers;
                break;
            default:
                ASSERT_NOT_REACHED();
                paintOrder = PaintOrderNormal;
            }
            svgStyle.setPaintOrder(static_cast<PaintOrder>(paintOrder));
            break;
        }
        // end of ident only properties
        case CSSPropertyFill:
        {
            if (isInherit) {
                const SVGRenderStyle& svgParentStyle = state.parentStyle()->svgStyle();
                svgStyle.setFillPaint(svgParentStyle.fillPaintType(), svgParentStyle.fillPaintColor(), svgParentStyle.fillPaintUri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
                return;
            }
            if (isInitial) {
                svgStyle.setFillPaint(SVGRenderStyle::initialFillPaintType(), SVGRenderStyle::initialFillPaintColor(), SVGRenderStyle::initialFillPaintUri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
                return;
            }
            if (is<SVGPaint>(*value)) {
                SVGPaint& svgPaint = downcast<SVGPaint>(*value);
                svgStyle.setFillPaint(svgPaint.paintType(), colorFromSVGColorCSSValue(&svgPaint, state.style()->color()), svgPaint.uri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
            }
            break;
        }
        case CSSPropertyStroke:
        {
            if (isInherit) {
                const SVGRenderStyle& svgParentStyle = state.parentStyle()->svgStyle();
                svgStyle.setStrokePaint(svgParentStyle.strokePaintType(), svgParentStyle.strokePaintColor(), svgParentStyle.strokePaintUri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
                return;
            }
            if (isInitial) {
                svgStyle.setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
                return;
            }
            if (is<SVGPaint>(*value)) {
                SVGPaint& svgPaint = downcast<SVGPaint>(*value);
                svgStyle.setStrokePaint(svgPaint.paintType(), colorFromSVGColorCSSValue(&svgPaint, state.style()->color()), svgPaint.uri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
            }
            break;
        }
        case CSSPropertyStrokeDasharray:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashArray, StrokeDashArray)
            if (!is<CSSValueList>(*value)) {
                svgStyle.setStrokeDashArray(SVGRenderStyle::initialStrokeDashArray());
                break;
            }

            CSSValueList& dashes = downcast<CSSValueList>(*value);

            Vector<SVGLength> array;
            size_t length = dashes.length();
            for (size_t i = 0; i < length; ++i) {
                CSSValue* currValue = dashes.itemWithoutBoundsCheck(i);
                if (!is<CSSPrimitiveValue>(*currValue))
                    continue;

                CSSPrimitiveValue* dash = downcast<CSSPrimitiveValue>(dashes.itemWithoutBoundsCheck(i));
                array.append(SVGLength::fromCSSPrimitiveValue(dash));
            }

            svgStyle.setStrokeDashArray(array);
            break;
        }
        case CSSPropertyFillOpacity:
        {
            HANDLE_INHERIT_AND_INITIAL(fillOpacity, FillOpacity)
            if (!primitiveValue)
                return;

            float f = 0.0f;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue() / 100.0f;
            else if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgStyle.setFillOpacity(f);
            break;
        }
        case CSSPropertyStrokeOpacity:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeOpacity, StrokeOpacity)
            if (!primitiveValue)
                return;

            float f = 0.0f;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue() / 100.0f;
            else if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgStyle.setStrokeOpacity(f);
            break;
        }
        case CSSPropertyStopOpacity:
        {
            HANDLE_INHERIT_AND_INITIAL(stopOpacity, StopOpacity)
            if (!primitiveValue)
                return;

            float f = 0.0f;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue() / 100.0f;
            else if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgStyle.setStopOpacity(f);
            break;
        }
        case CSSPropertyMarkerStart:
        {
            HANDLE_INHERIT_AND_INITIAL(markerStartResource, MarkerStartResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();

            svgStyle.setMarkerStartResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
            break;
        }
        case CSSPropertyMarkerMid:
        {
            HANDLE_INHERIT_AND_INITIAL(markerMidResource, MarkerMidResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();

            svgStyle.setMarkerMidResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
            break;
        }
        case CSSPropertyMarkerEnd:
        {
            HANDLE_INHERIT_AND_INITIAL(markerEndResource, MarkerEndResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();

            svgStyle.setMarkerEndResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
            break;
        }
        case CSSPropertyStrokeMiterlimit:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeMiterLimit, StrokeMiterLimit)
            if (!primitiveValue)
                return;

            float f = 0.0f;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgStyle.setStrokeMiterLimit(f);
            break;
        }
        case CSSPropertyFilter:
        {
            HANDLE_INHERIT_AND_INITIAL(filterResource, FilterResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();

            svgStyle.setFilterResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
            break;
        }
        case CSSPropertyMask:
        {
            HANDLE_INHERIT_AND_INITIAL(maskerResource, MaskerResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();

            svgStyle.setMaskerResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
            break;
        }
        case CSSPropertyClipPath:
        {
            HANDLE_INHERIT_AND_INITIAL(clipperResource, ClipperResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();

            svgStyle.setClipperResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
            break;
        }
        case CSSPropertyStopColor:
        {
            HANDLE_INHERIT_AND_INITIAL(stopColor, StopColor);
            if (is<SVGColor>(*value))
                svgStyle.setStopColor(colorFromSVGColorCSSValue(downcast<SVGColor>(value), state.style()->color()));
            break;
        }
       case CSSPropertyLightingColor:
        {
            HANDLE_INHERIT_AND_INITIAL(lightingColor, LightingColor);
            if (is<SVGColor>(*value))
                svgStyle.setLightingColor(colorFromSVGColorCSSValue(downcast<SVGColor>(value), state.style()->color()));
            break;
        }
        case CSSPropertyFloodOpacity:
        {
            HANDLE_INHERIT_AND_INITIAL(floodOpacity, FloodOpacity)
            if (!primitiveValue)
                return;

            float f = 0.0f;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue() / 100.0f;
            else if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgStyle.setFloodOpacity(f);
            break;
        }
        case CSSPropertyFloodColor:
        {
            HANDLE_INHERIT_AND_INITIAL(floodColor, FloodColor);
            if (is<SVGColor>(*value))
                svgStyle.setFloodColor(colorFromSVGColorCSSValue(downcast<SVGColor>(value), state.style()->color()));
            break;
        }
        case CSSPropertyGlyphOrientationHorizontal:
        {
            HANDLE_INHERIT_AND_INITIAL(glyphOrientationHorizontal, GlyphOrientationHorizontal)
            if (!primitiveValue)
                return;

            if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_DEG) {
                int orientation = angleToGlyphOrientation(primitiveValue->getFloatValue());
                ASSERT(orientation != -1);

                svgStyle.setGlyphOrientationHorizontal((EGlyphOrientation) orientation);
            }

            break;
        }
        case CSSPropertyGlyphOrientationVertical:
        {
            HANDLE_INHERIT_AND_INITIAL(glyphOrientationVertical, GlyphOrientationVertical)
            if (!primitiveValue)
                return;

            if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_DEG) {
                int orientation = angleToGlyphOrientation(primitiveValue->getFloatValue());
                ASSERT(orientation != -1);

                svgStyle.setGlyphOrientationVertical((EGlyphOrientation) orientation);
            } else if (primitiveValue->getValueID() == CSSValueAuto)
                svgStyle.setGlyphOrientationVertical(GO_AUTO);

            break;
        }
        case CSSPropertyEnableBackground:
            // Silently ignoring this property for now
            // http://bugs.webkit.org/show_bug.cgi?id=6022
            break;
        case CSSPropertyWebkitInitialLetter:
            // Not Implemented
            break;
        case CSSPropertyWebkitSvgShadow: {
            if (isInherit)
                return svgStyle.setShadow(state.parentStyle()->svgStyle().shadow() ? std::make_unique<ShadowData>(*state.parentStyle()->svgStyle().shadow()) : nullptr);
            if (isInitial || primitiveValue) // initial | none
                return svgStyle.setShadow(nullptr);

            if (!is<CSSValueList>(*value))
                return;

            CSSValueList& list = downcast<CSSValueList>(*value);
            if (!list.length())
                return;

            CSSValue* firstValue = list.itemWithoutBoundsCheck(0);
            if (!is<CSSShadowValue>(*firstValue))
                return;
            CSSShadowValue& item = downcast<CSSShadowValue>(*firstValue);
            IntPoint location(item.x->computeLength<int>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)),
                item.y->computeLength<int>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)));
            int blur = item.blur ? item.blur->computeLength<int>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)) : 0;
            Color color;
            if (item.color)
                color = colorFromPrimitiveValue(*item.color);

            // -webkit-svg-shadow does should not have a spread or style
            ASSERT(!item.spread);
            ASSERT(!item.style);

            auto shadowData = std::make_unique<ShadowData>(location, blur, 0, Normal, false, color.isValid() ? color : Color::transparent);
            svgStyle.setShadow(WTF::move(shadowData));
            return;
        }
        default:
            // If you crash here, it's because you added a css property and are not handling it
            // in either this switch statement or the one in StyleResolver::applyProperty
            ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", id);
            return;
    }
}

}
