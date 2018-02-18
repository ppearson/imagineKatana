#include "live_render_helpers.h"

#include <stdio.h>

#include "katana_helpers.h"

void LiveRenderHelpers::setUpdateXFormFromAttribute(const FnKat::GroupAttribute& xFormUpdateAttribute, KatanaUpdateItem& updateItem)
{
	updateItem.xform.resize(16);
	updateItem.haveXForm = true;

	FnKat::DoubleAttribute xformMatrixAttribute = xFormUpdateAttribute.getChildByName("global");			
	if (xformMatrixAttribute.isValid())
	{
		FnKat::DoubleConstVector matrixValues = xformMatrixAttribute.getNearestSample(0.0f);

		if (matrixValues.size() != 16)
			return;					
		
		std::copy(matrixValues.begin(), matrixValues.end(), updateItem.xform.begin());
	}
	else
	{
		// see if there's an interactive one
		
		FnKat::GroupAttribute xformInteractiveAttribute = xFormUpdateAttribute.getChildByName("interactive");
		if (xformInteractiveAttribute.isValid())
		{
			FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixStatic(xformInteractiveAttribute);
			const double* pMatrix = xforms[0].getValues();
			for (unsigned int i = 0; i < 16; i++)
			{
				updateItem.xform[i] = pMatrix[i];
			}
		}
	}
}
