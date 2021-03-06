/*
 * *****************************************************************************
 *   Copyright 2014-2015 Spectra Logic Corporation. All Rights Reserved.
 *   Licensed under the Apache License, Version 2.0 (the "License"). You may not
 *   use this file except in compliance with the License. A copy of the License
 *   is located at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   or in the "license" file accompanying this file.
 *   This file is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *   CONDITIONS OF ANY KIND, either express or implied. See the License for the
 *   specific language governing permissions and limitations under the License.
 * *****************************************************************************
 */

#include "lib/work_items/bulk_work_item.h"
#include "lib/work_items/object_work_item.h"

ObjectWorkItem::ObjectWorkItem(const QString& bucketName,
			       const QString& objectName,
			       const QString& fileName,
			       BulkWorkItem* bulkWorkItem)
	: WorkItem(),
	  m_bucketName(bucketName),
	  m_objectName(objectName),
	  m_file(fileName),
	  m_bulkWorkItem(bulkWorkItem)
{
}

ObjectWorkItem::~ObjectWorkItem()
{
	m_file.close();
}

size_t
ObjectWorkItem::ReadFile(char* data, size_t size, size_t count)
{
	size_t bytesRead = m_file.read(data, size * count);
	if (m_bulkWorkItem != NULL) {
		m_bulkWorkItem->UpdateBytesTransferred(bytesRead);
	}
	return bytesRead;
}

size_t
ObjectWorkItem::WriteFile(char* data, size_t size, size_t count)
{
	size_t bytesWritten = m_file.write(data, size * count);
	if (m_bulkWorkItem != NULL) {
		m_bulkWorkItem->UpdateBytesTransferred(bytesWritten);
	}
	return bytesWritten;
}
