/*
 ImagineKatana
 Copyright 2014-2019 Peter Pearson.

 Licensed under the Apache License, Version 2.0 (the "License");
 You may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ---------
*/

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
