/*
 * *****************************************************************************
 *   Copyright 2014 Spectra Logic Corporation. All Rights Reserved.
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

#ifndef BULK_GET_WORK_ITEM_H
#define BULK_GET_WORK_ITEM_H

#include <QList>
#include <QString>
#include <QUrl>

#include "lib/work_items/bulk_work_item.h"

// BulkGetWorkItem, a container class that stores all data necessary to perform
// a DS3 bulk put operation.
class BulkGetWorkItem : public BulkWorkItem
{
public:
	BulkGetWorkItem(const QString& host,
			const QList<QUrl> urls,
			const QString& destination);

	const QString GetDestination() const;
	Job::Type GetType() const;

private:
	void SortURLsByBucket();
	static bool CompareQUrls(const QUrl& a, const QUrl& b);

	QString m_destination;
};

inline const QString
BulkGetWorkItem::GetDestination() const
{
	return m_destination;
}

inline Job::Type
BulkGetWorkItem::GetType() const
{
	return Job::GET;
}

#endif