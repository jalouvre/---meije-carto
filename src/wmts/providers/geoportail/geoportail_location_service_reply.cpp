/***************************************************************************************************
**
** $qtcarto_BEGIN_LICENSE:GPL3$
**
** Copyright (C) 2016 Fabrice Salvaire
** Contact: http://www.fabrice-salvaire.fr
**
** This file is part of the QtCarto library.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
** $QTCARTO_END_LICENSE$
**
***************************************************************************************************/

/**************************************************************************************************/

#include "geoportail_location_service_reply.h"

/**************************************************************************************************/

QcGeoportailLocationServiceReply::QcGeoportailLocationServiceReply(QNetworkReply * reply,
                                                                   const QcLocationServiceQuery & query)
  : QcLocationServiceReply(reply, query)
{}

QcGeoportailLocationServiceReply::~QcGeoportailLocationServiceReply()
{}

// Handle a successful request : store image data
void
QcGeoportailLocationServiceReply::process_payload()
{
  qInfo() << network_reply()->readAll();
}

/**************************************************************************************************/

QcGeoportailLocationServiceReverseReply::QcGeoportailLocationServiceReverseReply(QNetworkReply * reply,
                                                                                 const QcLocationServiceReverseQuery & query)
  : QcLocationServiceReverseReply(reply, query)
{}

QcGeoportailLocationServiceReverseReply::~QcGeoportailLocationServiceReverseReply()
{}

// Handle a successful request : store image data
void
QcGeoportailLocationServiceReverseReply::process_payload()
{
  qInfo() << network_reply()->readAll();
}

/**************************************************************************************************/

// #include "geoportail_location_service_reply.moc"

/***************************************************************************************************
 *
 * End
 *
 **************************************************************************************************/
