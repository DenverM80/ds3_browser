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

#include <QDateTime>
#include <QFuture>
#include <QIcon>
#include <QMimeData>
#include <QSet>

#include "helpers/number_helper.h"
#include "lib/client.h"
#include "lib/logger.h"
#include "lib/watchers/get_service_watcher.h"
#include "models/ds3_browser_model.h"

// Must match m_rootItem->m_data;
enum Column { NAME, OWNER, SIZE, KIND, CREATED, COUNT };

static const QString REST_TIMESTAMP_FORMAT = "yyyy-MM-ddThh:mm:ss.000Z";
static const QString VIEW_TIMESTAMP_FORMAT = "MMMM d, yyyy h:mm AP";

static const QString BUCKET = "Bucket";
static const QString OBJECT = "Object";
static const QString FOLDER = "Folder";
static const QString BREAK = "Break";

//
// DS3BrowserItem
//

class DS3BrowserItem
{
public:
	DS3BrowserItem(const QList<QVariant>& data,
		       QString bucketName = QString(),
		       QString prefix = QString(),
		       DS3BrowserItem* parent = 0);
	~DS3BrowserItem();

	void AppendChild(DS3BrowserItem* item);
	bool GetCanFetchMore() const;
	QString GetBucketName() const;
	DS3BrowserItem* GetChild(int row) const;
	void RemoveChild(int row);
	int GetChildCount() const;
	int GetColumnCount() const;
	QVariant GetData(int column) const;
	uint32_t GetMaxKeys() const;
	QString GetNextMarker() const;
	QString GetPrefix() const;
	int GetRow() const;
	DS3BrowserItem* GetParent() const;
	bool IsFetching() const;
	void Reset();
	QString GetPath() const;

	void SetCanFetchMore(bool canFetchMore);
	void SetFetching(bool fetching);
	void SetMaxKeys(uint32_t maxKeys);
	void SetNextMarker(const QString nextMarker);

private:
	// m_canFetchMore only represents what DS3BrowserModel should report
	// for canFetchMore and not necessarily if the previous get
	// children request was truncated or not.
	bool m_canFetchMore;
	bool m_fetching;
	QList<DS3BrowserItem*> m_children;
	// List of data to show in the table.  Each item in the list
	// directly corresponds to a column.
	QList<QVariant> m_data;
	// So object items so they can easily keep track of what
	// bucket they're in.  For bucket items, this is the same as
	// m_data[0];
	const QString m_bucketName;
	uint32_t m_maxKeys;
	QString m_nextMarker;
	DS3BrowserItem* m_parent;
	// All parent folder object names, not including the bucket name
	const QString m_prefix;

	QList<DS3BrowserItem*> GetChildren() const;
};

DS3BrowserItem::DS3BrowserItem(const QList<QVariant>& data,
			       QString bucketName,
			       QString prefix,
			       DS3BrowserItem* parent)
	: m_canFetchMore(true),
	  m_fetching(false),
	  m_data(data),
	  m_bucketName(bucketName),
	  m_maxKeys(1000),
	  m_parent(parent),
	  m_prefix(prefix)
{
}

DS3BrowserItem::~DS3BrowserItem()
{
	qDeleteAll(m_children);
}

void
DS3BrowserItem::AppendChild(DS3BrowserItem* item)
{
	m_children << item;
}

inline QString
DS3BrowserItem::GetBucketName() const
{
	return m_bucketName;
}

inline bool
DS3BrowserItem::GetCanFetchMore() const
{
	return m_canFetchMore;
}

DS3BrowserItem*
DS3BrowserItem::GetChild(int row) const
{
	return GetChildren().value(row);
}

void
DS3BrowserItem::RemoveChild(int row)
{
	if (row < m_children.count()) {
		DS3BrowserItem* item = m_children.at(row);
		m_children.removeAt(row);
		delete item;
	}
}

int
DS3BrowserItem::GetChildCount() const
{
	return GetChildren().count();	
}

inline uint32_t
DS3BrowserItem::GetMaxKeys() const
{
	return m_maxKeys;
}

inline QString
DS3BrowserItem::GetNextMarker() const
{
	return m_nextMarker;
}

inline QString
DS3BrowserItem::GetPrefix() const
{
	return m_prefix;
}

int
DS3BrowserItem::GetRow() const
{
	if (m_parent) {
		return m_parent->GetChildren().indexOf(const_cast<DS3BrowserItem*>(this));
	}

	return 0;
}

int
DS3BrowserItem::GetColumnCount() const
{
	return m_data.count();
}

QVariant
DS3BrowserItem::GetData(int column) const
{
	QVariant data = m_data.value(column);
	if (column == SIZE && data != "Size" && data != "--") {
		qulonglong size = data.toULongLong();
		data = QVariant(NumberHelper::ToHumanSize(size));
	}
	return data;
}

inline DS3BrowserItem*
DS3BrowserItem::GetParent() const
{
	return m_parent;
}

inline bool
DS3BrowserItem::IsFetching() const
{
	return m_fetching;
}

void
DS3BrowserItem::Reset()
{
	qDeleteAll(m_children);
	m_children.clear();
	m_canFetchMore = true;
	m_nextMarker = QString();
}

QString
DS3BrowserItem::GetPath() const
{
	QString path = "/" + m_bucketName;
	if (GetData(KIND) == BUCKET) {
		return path;
	}

	if (m_prefix.isEmpty()) {
		path += "/";
	} else {
		path += "/" + m_prefix;
	}
	path += GetData(NAME).toString();
	return path;
}

QList<DS3BrowserItem*>
DS3BrowserItem::GetChildren() const
{
	return m_children;
}

inline void
DS3BrowserItem::SetCanFetchMore(bool canFetchMore)
{
	m_canFetchMore = canFetchMore;
}

inline void
DS3BrowserItem::SetFetching(bool fetching)
{
	m_fetching = fetching;
}

inline void
DS3BrowserItem::SetMaxKeys(uint32_t maxKeys)
{
	m_maxKeys = maxKeys;
}

inline void
DS3BrowserItem::SetNextMarker(const QString nextMarker)
{
	m_nextMarker = nextMarker;
}

//
// DS3BrowserModel
//

DS3BrowserModel::DS3BrowserModel(Client* client, QObject* parent)
	: QAbstractItemModel(parent),
	  m_client(client)
{
	// Headers must match DS3BrowserModel::Column
	QList<QVariant> column_names;
	column_names << "Name" << "Owner" << "Size" << "Kind" << "Created";
	m_rootItem = new DS3BrowserItem(column_names);
}

DS3BrowserModel::~DS3BrowserModel()
{
	delete m_rootItem;
}

bool
DS3BrowserModel::canFetchMore(const QModelIndex& parent) const
{
	DS3BrowserItem* parentItem;
	if (parent.isValid()) {
		parentItem = IndexToItem(parent);
	} else {
		parentItem = m_rootItem;
	}
	return parentItem->GetCanFetchMore();
}

int
DS3BrowserModel::columnCount(const QModelIndex& parent) const
{
	DS3BrowserItem* item;
	if (parent.isValid()) {
		item = IndexToItem(parent);
	} else {
		item = m_rootItem;
	}
	return item->GetColumnCount();
}

QVariant
DS3BrowserModel::data(const QModelIndex &index, int role) const
{
	DS3BrowserItem* item;
	QVariant data;

	if (!index.isValid()) {
		return data;
	}

	item = IndexToItem(index);
	int column = index.column();

	switch (role)
	{
	case Qt::DisplayRole:
		data = item->GetData(column);
		if (column == 0 && item->GetData(KIND) == BREAK) {
			m_view->setFirstColumnSpanned(index.row(), index.parent(), true);
		}
		break;
	case Qt::DecorationRole:
		if (column == NAME) {
			QVariant kind = item->GetData(KIND);
			if (kind == BUCKET) {
				data = QIcon(":/resources/icons/bucket.png");
			} else if (kind == FOLDER) {
				data = QIcon(":/resources/icons/files.png");
			} else if (kind == OBJECT) {
				data = QIcon(":/resources/icons/file.png");
			}
		}
		break;
	}
	return data;
}

bool
DS3BrowserModel::dropMimeData(const QMimeData* data,
			      Qt::DropAction /*action*/,
			      int /*row*/, int /*column*/,
			      const QModelIndex& parentIndex)
{
	DS3BrowserItem* parent = IndexToItem(parentIndex);
	QString bucketName = parent->GetBucketName();
	QString prefix = parent->GetPrefix();
	if (parent->GetData(KIND) != BUCKET) {
		prefix += parent->GetData(NAME).toString();
	}
	prefix.replace(QRegExp("^/"), "");
	QList<QUrl> urls = data->urls();
	m_client->BulkPut(bucketName, prefix, urls);
	return true;
}

Qt::ItemFlags
DS3BrowserModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags flags = QAbstractItemModel::flags(index);
	if (index.isValid()) {
		DS3BrowserItem* item = IndexToItem(index);
		QVariant kind = item->GetData(KIND);
		if (kind == BUCKET || kind == FOLDER) {
			flags |= Qt::ItemIsDropEnabled;
		}
	}
	return flags;
}

void
DS3BrowserModel::fetchMore(const QModelIndex& parent)
{
	bool parentIsValid = parent.isValid();
	DS3BrowserItem* parentItem;
	if (parent.isValid()) {
		parentItem = IndexToItem(parent);
	} else {
		parentItem = m_rootItem;
	}

	int lastRow = parentItem->GetChildCount() - 1;
	DS3BrowserItem* pageBreakItem = 0;
	if (lastRow >= 0) {
		DS3BrowserItem* lastChildItem = parentItem->GetChild(lastRow);
		if (lastChildItem->GetData(KIND) == BREAK) {
			pageBreakItem = lastChildItem;
		}
	}

	parentItem->SetFetching(true);
	parentIsValid ? FetchMoreObjects(parent) : FetchMoreBuckets(parent);

	if (pageBreakItem) {
		removeRow(lastRow, parent);
	}

	// Always set CanFetchMore to false so the view doesn't automatically
	// come right back around and ask to fetchMore when this model
	// emits the rowsInserted signal (which is the way
	// QAbstractItemView handles fetchMore).
	parentItem->SetCanFetchMore(false);
}

// rowCount actually determines whether or not the bucket has any objects in
// it.  hasChildren always returns true for buckets and folders so the
// caret is always displayed even if we don't yet know if the bucket/folder
// has any objects.
bool
DS3BrowserModel::hasChildren(const QModelIndex& parent) const
{
	if (!parent.isValid()) {
		return true;
	}

	DS3BrowserItem* item = IndexToItem(parent);
	QVariant kind = item->GetData(KIND);
	return (kind == BUCKET || kind == FOLDER);
}

QVariant
DS3BrowserModel::headerData(int section, Qt::Orientation /*orientation*/, int role) const
{
	if (role == Qt::DisplayRole) {
		return m_rootItem->GetData(section);
	}
	return QVariant();
}

QModelIndex
DS3BrowserModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent)) {
		return QModelIndex();
	}

	DS3BrowserItem* parentItem;

	if (parent.isValid()) {
		parentItem = IndexToItem(parent);
	} else {
		parentItem = m_rootItem;
	}

	DS3BrowserItem* childItem = parentItem->GetChild(row);
	if (childItem) {
		return createIndex(row, column, childItem);
	} else {
		return QModelIndex();
	}
}

QStringList
DS3BrowserModel::mimeTypes() const
{
	QStringList types = QAbstractItemModel::mimeTypes();
	types << "text/uri-list";
	return types;
}

QModelIndex
DS3BrowserModel::parent(const QModelIndex& index) const
{
	if (!index.isValid()) {
		return QModelIndex();
	}

	DS3BrowserItem* childItem = IndexToItem(index);
	DS3BrowserItem* parentItem = childItem->GetParent();

	if (parentItem == m_rootItem) {
		return QModelIndex();
	}

	return createIndex(parentItem->GetRow(), 0, parentItem);
}

bool
DS3BrowserModel::removeRows(int row, int count, const QModelIndex& parentIndex)
{
	if (row < 0 || count <= 0 || (row + count) > rowCount(parentIndex)) {
		return false;
	}

	DS3BrowserItem* parent = IndexToItem(parentIndex);

	beginRemoveRows(parentIndex, row, row + count - 1);

	for (int i = 0; i < count; i++) {
		parent->RemoveChild(row + i);
	}

	endRemoveRows();

	return true;
}

int
DS3BrowserModel::rowCount(const QModelIndex &parent) const
{
	DS3BrowserItem* parentItem;
	if (parent.column() > 0) {
		return 0;
	}

	if (parent.isValid()) {
		parentItem = IndexToItem(parent);
	} else {
		parentItem = m_rootItem;
	}

	return parentItem->GetChildCount();
}

bool
DS3BrowserModel::IsBucketOrFolder(const QModelIndex& index) const
{
	DS3BrowserItem* item = IndexToItem(index);
	QVariant kind = item->GetData(KIND);
	return (kind == BUCKET || kind == FOLDER);
}

bool
DS3BrowserModel::IsBreak(const QModelIndex& index) const
{
	DS3BrowserItem* item = IndexToItem(index);
	QVariant kind = item->GetData(KIND);
	return (kind == BREAK);
}

bool
DS3BrowserModel::IsFetching(const QModelIndex& parent) const
{
	DS3BrowserItem* parentItem;
	if (parent.isValid()) {
		parentItem = IndexToItem(parent);
	} else {
		parentItem = m_rootItem;
	}
	return parentItem->IsFetching();
}

QString
DS3BrowserModel::GetPath(const QModelIndex& index) const
{
	QString path = "/";
	DS3BrowserItem* item = IndexToItem(index);
	if (item != NULL) {
		path = item->GetPath();
	}
	return path;
}

void
DS3BrowserModel::Refresh(const QModelIndex& index)
{
	DS3BrowserItem* item;
	if (index.isValid()) {
		item = IndexToItem(index);
	} else {
		item = m_rootItem;
	}

	beginResetModel();
	item->Reset();
	endResetModel();
}

void
DS3BrowserModel::FetchMoreBuckets(const QModelIndex& parent)
{
	GetServiceWatcher* watcher = new GetServiceWatcher(parent);
	connect(watcher, SIGNAL(finished()), this, SLOT(HandleGetServiceResponse()));
	QFuture<ds3_get_service_response*> future = m_client->GetService();
	watcher->setFuture(future);
}

void
DS3BrowserModel::FetchMoreObjects(const QModelIndex& parent)
{
	DS3BrowserItem* parentItem;
	if (parent.isValid()) {
		parentItem = IndexToItem(parent);
	} else {
		// Should never get here since we should never try to
		// fetch objects at the root level.
		parentItem = m_rootItem;
	}

	bool isBucket = parentItem->GetData(KIND) == BUCKET;
	QString name = parentItem->GetData(NAME).toString();
	QString bucketName = parentItem->GetBucketName();
	QString prefix = parentItem->GetPrefix();
	if (!isBucket) {
		prefix += name + "/";
	}

	uint32_t maxKeys = parentItem->GetMaxKeys();
	QString nextMarker = parentItem->GetNextMarker();
	ds3_get_bucket_response* response = m_client->GetBucket(bucketName,
								prefix,
								"/",
								nextMarker,
								maxKeys);

	QVariant owner = parentItem->GetData(OWNER);

	QSet<QString> currentCommonPrefixNames;
	for (int i = 0; i < parentItem->GetChildCount(); i++) {
		DS3BrowserItem* object = parentItem->GetChild(i);
		if (object->GetData(KIND) == FOLDER) {
			currentCommonPrefixNames << object->GetData(NAME).toString();
		}
	}

	QList<DS3BrowserItem*> newChildren;

	for (size_t i = 0; i < response->num_common_prefixes; i++) {
		ds3_str* rawCommonPrefix = response->common_prefixes[i];
		// The order in which bucketData is filled must match
		// Column
		QList<QVariant> objectData;
		DS3BrowserItem* object;

		QString nextName = QString(QLatin1String(rawCommonPrefix->value));
		nextName.replace(QRegExp("^" + prefix), "");
		nextName.replace(QRegExp("/$"), "");
		if (!currentCommonPrefixNames.contains(nextName)) {
			objectData << nextName;
			objectData << owner;
			objectData << "--";
			objectData << FOLDER;
			objectData << "--";
			object = new DS3BrowserItem(objectData,
						    bucketName,
						    prefix,
						    parentItem);
			newChildren << object;
		}
	}

	for (size_t i = 0; i < response->num_objects; i++) {
		QString nextName;
		char* rawCreated;
		QDateTime createdDT;
		QString created;
		// The order in which bucketData is filled must match
		// Column
		QList<QVariant> objectData;
		DS3BrowserItem* object;

		ds3_object rawObject = response->objects[i];

		nextName = QString(QLatin1String(rawObject.name->value));
		if (nextName == prefix) {
			continue;
		}
		nextName.replace(QRegExp("^" + prefix), "");
		objectData << nextName;

		objectData << owner;

		// TODO Humanize the size
		objectData << rawObject.size;

		objectData << OBJECT;

		if (rawObject.last_modified) {
			rawCreated = rawObject.last_modified->value;
			createdDT = QDateTime::fromString(QString(QLatin1String(rawCreated)),
							  REST_TIMESTAMP_FORMAT);
			created = createdDT.toString(VIEW_TIMESTAMP_FORMAT);
		}
		objectData << created;

		object = new DS3BrowserItem(objectData,
					    bucketName,
					    prefix,
					    parentItem);
		newChildren << object;
	}

	if (response->next_marker) {
		parentItem->SetNextMarker(QString(QLatin1String(response->next_marker->value)));
	}
	parentItem->SetMaxKeys(response->max_keys);

	if (response->is_truncated) {
		QList<QVariant> pageBreakData;
		pageBreakData << "Click to load more" << "" << "" << BREAK;
		DS3BrowserItem* pageBreak = new DS3BrowserItem(pageBreakData,
							       bucketName,
							       prefix,
							       parentItem);
		newChildren << pageBreak;
	}

	int numNewChildren = newChildren.count();
	if (numNewChildren > 0) {
		int startRow = rowCount(parent);
		beginInsertRows(parent, startRow, startRow + numNewChildren - 1);
		for (int i = 0; i < numNewChildren; i++) {
			parentItem->AppendChild(newChildren.at(i));
		}
		endInsertRows();
	}

	ds3_free_bucket_response(response);
}

void
DS3BrowserModel::HandleGetServiceResponse()
{
	LOG_DEBUG("HandleGetServiceResponse");

	GetServiceWatcher* watcher = static_cast<GetServiceWatcher*>(sender());
	const QModelIndex& parent = watcher->GetParentModelIndex();
	ds3_get_service_response* response = watcher->result();

	DS3BrowserItem* parentItem;
	if (parent.isValid()) {
		// Should never get here since we should never try to fetch
		// buckets at the bucket level.
		parentItem = IndexToItem(parent);
	} else {
		parentItem = m_rootItem;
	}

	QString owner = QString(QLatin1String(response->owner->name->value));

	size_t numBuckets = response->num_buckets;
	if (numBuckets > 0) {
		int startRow = rowCount(parent);
		beginInsertRows(parent, startRow, startRow + numBuckets - 1);
	}

	for (size_t i = 0; i < response->num_buckets; i++) {
		QString name;
		char* rawCreated;
		QDateTime createdDT;
		QString created;
		// The order in which bucketData is filled must match
		// Column
		QList<QVariant> bucketData;
		DS3BrowserItem* bucket;

		ds3_bucket rawBucket = response->buckets[i];

		name = QString(QLatin1String(rawBucket.name->value));
		bucketData << name;
		bucketData << owner;
		bucketData << "--";
		bucketData << BUCKET;

		rawCreated = rawBucket.creation_date->value;
		createdDT = QDateTime::fromString(QString(QLatin1String(rawCreated)),
						  REST_TIMESTAMP_FORMAT);
		created = createdDT.toString(VIEW_TIMESTAMP_FORMAT);
		bucketData << created;

		bucket = new DS3BrowserItem(bucketData,
					    name,
					    QString(),
					    parentItem);
		parentItem->AppendChild(bucket);
	}

	if (numBuckets > 0) {
		endInsertRows();
	}

	ds3_free_service_response(response);
	delete watcher;
	parentItem->SetFetching(false);
}
