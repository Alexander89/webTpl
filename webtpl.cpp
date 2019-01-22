#include "webtpl.h"
//#include "mobileinterface.h"
#include "gaugele_connect.h"
#include <QBitArray>
#include <QDateTime>
#include <QHostAddress>
#include <QRect>
#include <QSize>
#include <qdatastream.h>
#include <zlib.h>

//#define traceTag(x)  qDebug() << x
//#define traceMath(x)  qDebug() << x
//#define traceMath1(x)  traceMath(x)
//#define traceMath2(x) qDebug() << x
//#define traceMath3(x) qDebug() << x
#ifndef traceTag
#define traceTag(x)
#endif
#ifndef traceMath
#define traceMath(x)
#define traceMath1(x)
#endif
#ifndef traceMath2
#define traceMath2(x)
#endif
#ifndef traceMath3
#define traceMath3(x)
#endif

namespace WebTpl {

WebTpl::WebTpl(QObject *parent) :
    QObject(parent)
{
    m_var["TRUE"]="1";
    m_var["FALSE"]="0";

}

QString WebTpl::applyTplSyntax(QString html, uint start, uint line)
{
    int nextTag = html.indexOf("<$", start);
    line += html.mid(start, nextTag-start).count('\n');
    if ( nextTag == -1 )
        return html;
    int endTag = html.indexOf( "$>", nextTag )+2;
    if ( endTag == 0 )
    {
        qDebug() << "end not found";
        return html;
    }

    int tplTagLng = endTag - nextTag;
    WebTplTag tag( this, html.mid( nextTag, tplTagLng ), nextTag);

    switch ( tag.type() )
    {
    case t_tpl: {
        if ( ifLVL() > 0 && getIfLVL().disabledSection() )
            nextTag = tag.removeTag(html); //html = html.remove( tag.pos(), tag.lng() );
        else
            html = html.replace( tag.tag(), loadTplFile( tag.attr() ) );
        break;
        }

    case t_if:
    case t_elseif:
    case t_else:
    case t_ifend:
        if ( (m_foreachInfo.size() && getForeachLVL().valDisabled()) ||
             (m_forInfo.size() && getForLVL().valDisabled()) )
        {            
            nextTag+=tag.lng();
            break;
        }

        //qDebug() << nextTag <<"iftag "<< tag.pos()<< "" << tag.tag().leftJustified(50,' ',true) << "in line: " << line << " lvl: " << ifLVL()+1;
        html = applyIF(html, tag, nextTag);
        break;

    case t_foreach:
    case t_foreachend:
        if ( (ifLVL() >= 0 && getIfLVL().disabledSection()) ||
             (m_forInfo.size() && getForLVL().valDisabled()) )
            nextTag += tag.lng();
        else
            html = applyForeach(html, tag, nextTag);
        break;

    case t_for:
    case t_forend:
        if ( (ifLVL() >= 0 && getIfLVL().disabledSection() ) ||
            (foreachLVL() >= 0 && getForeachLVL().valDisabled()) )
            nextTag += tag.lng();
        else
            html = applyFor(html, tag, nextTag);
        break;

    case t_func:
        if ( (ifLVL() >= 0 && getIfLVL().disabledSection()) ||
             (m_foreachInfo.size() && getForeachLVL().valDisabled()) ||
             (m_forInfo.size() && getForLVL().valDisabled()) )
        {
            nextTag+=tag.lng();
            break;
        }
        html = applyFunc(html, tag, nextTag);
        break;

    case t_maths: {

        if ( (m_foreachInfo.size() && getForeachLVL().valDisabled()) ||
             (m_forInfo.size() && getForLVL().valDisabled()) )
        {
            nextTag+=tag.lng();
            break;
        }

        WebTplVarMath var(this);
        html = var.parseVar(html, nextTag, tag.lng());
    }
        break;

    default:
        qDebug() << "default"  << tag.tag();
        html.insert( nextTag + tag.tag().size(), " << unexpected // not-implemented tag --!>");
        html.insert( nextTag, "<!-- ");
        nextTag += tag.tag().size() + 25;
        break;
    }

    html = applyTplSyntax( html, nextTag, line );

    return html;
}
QString WebTpl::applyIF(QString html, WebTplTag &tag, int &nextTag)
{
    switch ( tag.type() )
    {
    case t_if: {
        m_ifInfo.append(IfInfo());
        nextTag = tag.removeTag(html);

        if ( ifLVL() && getLastIfLVL().disabledSection() )          // in einem gelöschten Bereich, -> bereich muss nicht validiert werden
        {
            getIfLVL().m_removeStart = getLastIfLVL().m_removeStart;       // gesetzt, um untergeordnete bereiche auch zu "deaktivieren"
            getIfLVL().m_hasValidPart = true;        // alle anderen Teile dieses If-falles werden auch gelöscht!!
            break;
        }
        if ( tag.stepInto() )        
            getIfLVL().m_hasValidPart = true;        
        else
            getIfLVL().m_removeStart = tag.pos();        // wird dieser IF bereich nicht bereits gelöcht, wird hier angefangen den bereich zu löschen
        break;
    }
    case t_elseif: {
        if ( ifLVL() < 0 )
        {
            html.insert( nextTag + tag.lng(), " << missing IF-tag --!>");
            html.insert( nextTag, "<!-- ");
            nextTag += tag.lng() + 25;
            break;
        }

        // Gibt es einen validen Bereich, dann sind alle späteren bereiche unintresant
        if ( getIfLVL().m_hasValidPart )
        {
            if ( !getIfLVL().disabledSection() )  // wird dieser IF bereich nicht bereits gelöcht, wird hier angefangen den bereich zu löschen
                getIfLVL().m_removeStart = nextTag;

            nextTag += tag.lng();
            break;  // tag muss hier nicht gelöscht werden, da der ganze Bereich beim ENDIF gelöscht wird
        }

        if ( tag.stepInto() ) // ist dieser Bereich ok, dann wird der vorherige Teil (incl. tag selbst) gelöscht, sowie der rest bis zum endif
        {
            int removedLng = (nextTag - getIfLVL().m_removeStart);
            html = html.remove(getIfLVL().m_removeStart, removedLng + tag.lng() );

            getIfLVL().reset();
            getIfLVL().m_hasValidPart = true;
            nextTag -= removedLng;
        }
        else
        {
            if ( !getIfLVL().disabledSection() )  // wird dieser IF bereich nicht bereits gelöcht, wird hier angefangen den bereich zu löschen
                getIfLVL().m_removeStart = tag.pos();
            nextTag += tag.lng();
        }

        break;
    }
    case t_else:
        if ( ifLVL() < 0 )
        {
            html.insert( nextTag + tag.lng(), " << missing IF-tag --!>");
            html.insert( nextTag, "<!-- ");
            nextTag += tag.lng() + 25;
            break;
        }

        // Gibt es einen validen Bereich, dann sind alle späteren bereiche unintresant
        //html = html.remove(tag.pos(), tag.lng()); // nur endtag löschen.
        if ( getIfLVL().m_hasValidPart )
        {
            if ( !getIfLVL().disabledSection() )  // wird dieser IF bereich nicht bereits gelöcht, wird hier angefangen den bereich zu löschen
                getIfLVL().m_removeStart = nextTag;
            // tag muss hier nicht gelöscht werden, da der ganze bereich beim endif gelöscht wird.
            nextTag += tag.lng();
            break;
        }
        else
        {
            int removedLng = (nextTag - getIfLVL().m_removeStart) ;
            html = html.remove( getIfLVL().m_removeStart, removedLng + tag.lng());
            nextTag = getIfLVL().m_removeStart;
            getIfLVL().reset();
            getIfLVL().m_hasValidPart = true; // eigentlich nicht nötig, da es nur else gibt. Evtl. bei fehlerhafter programmierung. So ist es besser das auszuschließen.
        }
        break;

    case t_ifend:
        if ( ifLVL() < 0 )
        {
            html.insert( nextTag + tag.lng(), " << missing IF-tag --!>");
            html.insert( nextTag, "<!-- ");
            nextTag += tag.lng() + 25;
            break;
        }

        if ( getIfLVL().disabledSection() ) // ist die letzte Section ungültig, dann muss diese incl. endtag gelöscht werden.
        {
            int removeLng = (nextTag - getIfLVL().m_removeStart);
            html = html.remove( getIfLVL().m_removeStart, removeLng + tag.lng() );
            nextTag -= removeLng;
        }
        else
            nextTag = tag.removeTag(html);  //html = html.remove(tag.pos(), tag.lng()); // nur endtag löschen.


        m_ifInfo.takeLast();
        break;

    default:
        qDebug() << "No If statement! " << nextTag << " " << tag.tag();
        nextTag = tag.removeTag(html);
        //html = html.remove(tag.pos(), tag.lng()); // nur endtag löschen.
        break;
    }
    return html;
}
QString WebTpl::applyFor(QString html, WebTplTag &tag, int &nextTag)
{
    switch ( tag.type() )
    {
    case t_for: {
        m_forInfo.append(ForInfo());
        if ( forLVL() && getLastForLVL().valDisabled() )
        {
            nextTag += tag.lng();
            return html;
        }
        getForLVL().m_tag = &tag;
        int varPos = tag.attr().indexOf("=");
        if ( varPos == -1 )
            break;
        getForLVL().m_key = tag.attr().left(varPos).trimmed();

        int toPos = tag.attr().indexOf("->", varPos + 1);
        if ( toPos == -1 )
            break;

        WebTplVarMath maths(this);
        getForLVL().m_start = maths.parseMaths( tag.attr().mid(varPos + 1, toPos-( varPos + 1) )).toInt();

        int step = tag.attr().indexOf(":", toPos + 2);
        if ( step != -1 )
        {
            getForLVL().m_end = maths.parseMaths( tag.attr().mid(toPos + 2, step - (  toPos + 2 ) )).toInt();
            getForLVL().m_step = maths.parseMaths( tag.attr().mid(step +  1) ).toInt();
        }
        else
        {
            getForLVL().m_end = maths.parseMaths( tag.attr().mid(toPos + 2) ).toInt();
            if ( getForLVL().m_end > getForLVL().m_start )
                getForLVL().m_step=1;
            else
                getForLVL().m_step=-1;
        }
        tag.removeTag(html);
        break;
    }
    case t_forend:
    {
        if ( forLVL() < 0 )
        {
            html.insert( nextTag + tag.lng(), " << missing For-tag --!>");
            html.insert( nextTag, "<!-- ");
            nextTag += tag.lng() + 25;
            break;
        }
        if ( forLVL() && getLastForLVL().valDisabled() )
        {
            m_forInfo.removeLast();
            nextTag += tag.lng();
            break;
        }

        ForInfo &info = m_forInfo.last();
        info.m_disableValidation = false;

        int start = info.m_tag->pos();
        QString content = html.mid( start, tag.pos() - start);
        QString forresult;

        for( info.m_value = info.m_start; info.m_value != info.m_end; info.m_value += info.m_step )
        {
            QString section = content;
            section = section.replace(info.m_key, QString::number(info.m_value) );

            section = applyTplSyntax(section);
            forresult.append(section);
        }
        html = html.left( start ) + forresult + html.mid(tag.pos()+tag.lng());
        nextTag = start + forresult.size();
        m_forInfo.takeLast();
        return html;
    }
    default:
        qDebug() << "No For statement! " << tag;
        break;
    }

    return html;
}
QString WebTpl::applyForeach(QString html, WebTplTag &tag, int &nextTag)
{
    switch ( tag.type() )
    {
    case t_foreach: {
        m_foreachInfo.append(ForeachInfo());
        if ( foreachLVL() && getLastForeachLVL().valDisabled() )
        {
            nextTag += tag.lng();
            return html;
        }
        getForeachLVL().m_tag = &tag;
        int src = tag.attr().indexOf("=>");
        if ( src == -1 )
            break;
        QString source = tag.attr().left(src);
        QString value = tag.attr().mid( src + 2 ).trimmed();
        QString key;

        int keyPos = value.indexOf(',');
        if ( keyPos != -1 )
        {
            key = value.left(keyPos).trimmed();
            value = value.mid(keyPos+1).trimmed();
        }
        getForeachLVL().m_key = key;
        getForeachLVL().m_value = value;
        getForeachLVL().m_disableValidation = true;

        auto list = getList(source);
        for (int i = 0;i < list.size(); ++i)
        {
            getForeachLVL().m_keys.append( list[i].first );
            getForeachLVL().m_values.append( list[i].second );
        }
        break;
    }
    case t_foreachend:
    {
        if ( foreachLVL() < 0 )
        {
            html.insert( nextTag + tag.lng(), " << missing Foreach-tag --!>");
            html.insert( nextTag, "<!-- ");
            nextTag += tag.lng() + 25;
            break;
        }
        if ( foreachLVL() && getLastForeachLVL().valDisabled() )
        {
            m_foreachInfo.removeLast();
            nextTag += tag.lng();
            break;
        }

        ForeachInfo &info = m_foreachInfo.last();
        info.m_disableValidation = false;

        int start = info.m_tag->pos();
        QString content = html.mid( start, tag.pos() - start);
        QString forresult;



        for( int i = 0; i < info.m_keys.size(); ++i)
        {
            QString section = content;
            if (info.m_key.size())
                section = section.replace(info.m_key, info.m_keys[i].remove('"'));
            if (info.m_value.size())
                section = section.replace(info.m_value, info.m_values[i].remove('"'));

            section = applyTplSyntax(section);
            forresult.append(section);
        }
        html = html.left( start ) + forresult + html.mid(tag.pos()+tag.lng());
        nextTag = start + forresult.size();
        m_foreachInfo.takeLast();
        return html;
    }
    default:
        qDebug() << "No Foreach statement! " << tag;
        break;
    }

    tag.removeTag(html);
    return html;
}
QString WebTpl::applyFunc(QString html, WebTplTag &tag, int &nextTag)
{
    switch ( tag.type() )
    {
    case t_func: {

        if ( !tag.attr().compare("vardump()", Qt::CaseInsensitive) )
        {
            QString dump = dumpVar();
            nextTag+= dump.size();
            return html.replace( tag.tag(), dump );
        }
        QString out = "<!-- unknown function: "+ tag.attr() + "-->";
        nextTag+= out.size();
        return html.replace( tag.tag(), out );
    }
        break;

    default:
        qDebug() << "No function statement! " << tag;
        break;
    }

    nextTag = tag.removeTag(html);
    return html;
}
QString WebTpl::loadTplFile(QString path)
{
    int varpos = path.indexOf("%");
    while (varpos != -1)
    {
        int end = path.indexOf("%", varpos+1);
        QString vartext =  path.mid(varpos+1, end-varpos-1);
        WebTplVarMath var( this );
        QString result = var.parseMaths(vartext);
        path = path.replace(varpos, end-varpos+1, result);
        traceTag( result << " " << path );
        varpos = path.indexOf("%",end+1);
    }

    QString html = g_app->readRcqFile(path);
    if ( html.isEmpty() )
        return QString("<!--"+ path +" not Found //-->");

    html = applyTplSyntax(html);

    return html;
}

/**
 * @brief lädt eine z.b.: *.tpl, parsed diese und convertiert sie als QByteArray (UTF8)
 * @param file  Dateiname der zu ladenden Datei
 * @return QByteArray mit dem UTF8-Codierten und geparten inhalt der Datei
 */
QByteArray WebTpl::createHTML( QString file )
{
    //dumpVar();
    QString html = loadTplFile( file );
    return html.toUtf8();
}
/**
 * @brief Setter für Variablen, die im Pemplate verwendet werden können.
 * Unterstützte Datentypen:
 *      Bool\n
 *      Int         UInt\n
 *      Long        ULong\n
 *      LongLong    ULongLong\n
 *      Double\n
 *      Char\n
 *      String\n
 *      Map\n
 *      List        StringList\n
 *      ByteArray\n
 *      BitArray\n
 *      Date        Time        DateTime\n
 *      Rect        RectF\n
 *      Size        SizeF\n
 *      Point       PointF\n
 *
 * @param name
 * @param value
 */
void WebTpl::publishVar(QString name, const QVariant value)
{
    //qDebug() << " publish " << name << " with " << value;
    QString text;
    switch ((int)value.type())
    {
    case QVariant::Invalid:
        qDebug() << "Invalid QVariant: " << name ;
        return;

    case QMetaType::Bool:       text = value.toBool() ? "1" : "0";  break;
    case QVariant::Int:         text = QString::number( value.toInt() );       break;
    case QVariant::UInt:        text = QString::number( value.toUInt() );      break;
    case QMetaType::Long:       text = QString::number( value.toInt() );       break;
    case QMetaType::ULong:      text = QString::number( value.toUInt() );      break;
    case QVariant::LongLong:    text = QString::number( value.toLongLong() );  break;
    case QVariant::ULongLong:   text = QString::number( value.toULongLong() ); break;
    case QVariant::Double:      text = QString::number( value.toDouble() );    break;
    case QVariant::Char:
    case QVariant::String:      text = "\"" + value.toString() + "\""; break;

    case QVariant::Map: {
        QMap<QString, QVariant> map = value.toMap();
        m_var[name+".size"] = QString::number( map.size() );
        auto itr = map.constBegin();
        while ( itr != map.constEnd() )
        {
            publishVar(name + "[" + itr.key() + "]", itr.value() );
            itr++;
        }
        return;
    }
    case QVariant::List: {
        QList<QVariant> list = value.toList();
        m_var[name+".size"] = QString::number( list.size() );
        for ( int i = 0; i < list.size(); i++)
            publishVar( QString("%1[%2]").arg(name).arg(i), list.at(i) );
        return;
    }
    case QVariant::StringList:{
        QStringList list = value.toStringList();
        m_var[name+".size"] = QString::number( list.size() );
        for ( int i = 0; i < list.size(); i++)
            m_var[QString("%1[%2]").arg(name).arg(i)] = "\"" + list.at(i) + "\"";
        return;
    }

    case QVariant::ByteArray:   text = QString::fromLatin1( value.toByteArray() ); break;
    case QVariant::BitArray:{
        QBitArray array = value.toBitArray();
        for (int i = 0; i < array.size(); ++i)
            m_var[QString("%0[%1]").arg(name).arg(i) ] = array.testBit(i) ? "true" : "false";
        return;
    }
    case QVariant::Date:{
        m_var[name] = value.toDate().toString("dd.MM.yyyy");
        m_var[name+"-RCF"] = value.toDate().toString("yyyy-MM-dd");
        return;
    }
    case QVariant::Time:{
        m_var[name] = value.toTime().toString("HH:mm:ss");
        m_var[name+"-RCF"] = value.toDate().toString("HH:mm");
        return;
    }
    case QVariant::DateTime:{
        m_var[name] = value.toDateTime().toString("dd.MM.yyyy HH:mm:ss");
        m_var[name+"-RCF"] = value.toDateTime().toString("yyyy-MM-ddTHH:mm:ss");
        return;
    }

    case QVariant::Rect: {
        QRect r = value.toRect();
        m_var[name+"->left"] = QString::number( r.left() );
        m_var[name+"->top"] = QString::number( r.top() );
        m_var[name+"->right"] = QString::number( r.right() );
        m_var[name+"->bottom"] = QString::number( r.bottom() );

        m_var[name+"->width"] = QString::number( r.width() );
        m_var[name+"->height"] = QString::number( r.height() );
        return;
    }
    case QVariant::RectF: {
        QRectF r = value.toRectF();
        m_var[name+"->left"] = QString::number( r.left() );
        m_var[name+"->top"] = QString::number( r.top() );
        m_var[name+"->right"] = QString::number( r.right() );
        m_var[name+"->bottom"] = QString::number( r.bottom() );

        m_var[name+"->width"] = QString::number( r.width() );
        m_var[name+"->height"] = QString::number( r.height() );
        return;
    }
    case QVariant::Size:{
        QSize s = value.toSize();
        m_var[name+"->width"] = QString::number(  s.width() );
        m_var[name+"->height"] = QString::number( s.height() );
        return;
    }
    case QVariant::SizeF:{
        QSizeF s = value.toSizeF();
        m_var[name+"->width"] = QString::number(  s.width() );
        m_var[name+"->height"] = QString::number( s.height() );
        return;
    }
    case QVariant::Point: {
        QPoint p = value.toPoint();
        m_var[name+"->x"] = QString::number( p.x() );
        m_var[name+"->y"] = QString::number( p.y() );
        m_var[name+"->length"] = QString::number( p.manhattanLength() );
        return;
    }
    case QVariant::PointF: {
        QPointF p = value.toPointF();
        m_var[name+"->x"] = QString::number( p.x() );
        m_var[name+"->y"] = QString::number( p.y() );
        m_var[name+"->length"] = QString::number( p.manhattanLength() );
        return;
    }
    default:
        qDebug() << name << " type not supported: " << value.type() ;
        return;
    }

    // simpleTypes werden hier gemeinsam in die Map geschrieben

    m_var[name] = text;
}
/**
 * @brief überprüft, ob eine Variable gesetzt wurde
 * @param varname   Name der Variable
 * @return true wenn gesetzt, sonst false
 */
bool WebTpl::isset(QString varname) const
{
    return m_var.contains(varname);
}
/**
 * @brief gibt den wert einer Variablen an
 * @param varname   Name der Variable
 * @return wert der Variable. existiert diese nicht wird Sie mit "" angelegt
 */
QString WebTpl::var(QString varname) const
{
    return m_var[varname];
}
QList<QPair<QString, QString> > WebTpl::getList(QString varname) const
{
    QList<QPair<QString, QString> > list;
    varname = varname.trimmed();

    auto itr = m_var.constBegin();
    while ( itr != m_var.constEnd() )
    {
        QString key = itr.key();
        if ( key.startsWith( varname ) )
        {
            key = key.remove(varname);
            if ( key.count('[') == 1 )
            {
                int start = key.indexOf("[")+1;
                key = key.mid(start, (key.indexOf("]") - start));
                list.append(QPair<QString, QString>(key, itr.value()) );
            }
        }
        itr++;
    }
    return list;
}
/**
 * @brief gibt alle Variablen die aktuell existieren in qDebug aus und im document gekapselt in einem pre Tag
 * @return  pre-Tag (html) mit allen variablen die gesetzt wurden
 */
QString WebTpl::dumpVar() const
{
    QString out;
    QTextStream ts(&out);
    ts << m_var.size() << "\n";
    auto itr = m_var.constBegin();
    while ( itr != m_var.constEnd() )
    {
        ts << itr.key() << " = " << itr.value() << "\n";
        itr++;
    }
    QDebug dbg = qDebug() ;
    dbg.noquote();
    dbg.nospace();
    dbg << out;
    out = "<pre>"+out+"</pre>";
    return out;
}
/**
 * @brief vonvertiert alle Variablen is ein JSON kompatible format.
 *
 * Sollten Variablen nicht convertert werden, so müssen Sie nach diesem Aufruf gesetzt werden
 */
void WebTpl::convertAllVarToJSON()
{
    auto itr = m_var.begin();
    while (itr != m_var.end())
    {
        itr.value() = itr.value().replace("\\", "\\\\");
        itr++;
    }
}

/**
 * @brief Convertiert einen 32 Bit RGBA codierten Wert in einen HTML-Std Wert
 * @return z.B.: 31762 -> "#007c12"
 */
QString WebTpl::convertColor(uint color)
{
    return "#"+QString("%1").arg(color,6,16, QChar('0') );
}
/**
 * @brief Überpfrüft ob die angegebene Condition bereits validiert wurde.
 * @param condition Condition die überprüft werden soll
 * @return true, wenn diese Bedingung bereits validiert wurde, sonst false
 */
bool WebTpl::isValidationCached(QString condition)
{
    return m_validationCache.contains(condition);
}
/**
 * @brief fragt das ergebnis einer Bedingung aus dem Cache ab.
 * @param condition Bedingung die abgefragt wird
 * @return ist die Bedingung noch nicht validiert, wird "" zurück gegeben
 */
QString WebTpl::getValidation(QString condition)
{
    if ( isValidationCached(condition) )
        return m_validationCache[condition];
    return "";
}
/**
 * @brief Wurde eine Validierung durchgeführt, wird diese gecached um beim nächsten mal schneller zu sein
 * @param condition Bedingung die gelöst wurde
 * @param value ergebnis der Validierung
 */
void WebTpl::setValidation(QString condition, QString value)
{
    m_validationCache[condition] = value;
}




WebTplTag::WebTplTag(WebTpl *tpl, QString tag, uint pos) :
    m_tpl(tpl),
    m_type(t_undef),
    m_tagPos(pos),
    m_validated(false),
    m_true(false)
{
    parseTagInfo(tag);
}
WebTplTag::~WebTplTag()
{
    //qDebug() << "deleting " << tag();
}
void WebTplTag::parseTagInfo(QString tag)
{
    tag = tag.trimmed();
    if (tag.size() < 5)
        return;

    m_fullTag = tag;

    if (tag.startsWith("<$"))
    {
        int condition = tag.indexOf(' ',3)+1;   // +1 to skip ' '
        if ( condition != 0 )
        {
            m_tagname = tag.mid(2, condition - 3);
            m_attr = tag.mid( condition, m_fullTag.size() - condition - 2 );
        }
        else
            m_tagname = tag.mid(2, m_fullTag.size()-4);

        parseTagTpl(m_tagname);
    }
    traceTag(*this);
}
TagType WebTplTag::parseTagTpl(QString tagName)
{
    tagName = tagName.trimmed();

    if ( !tagName.compare("tpl", Qt::CaseInsensitive) )
        m_type = t_tpl;
    else if ( !tagName.compare("if", Qt::CaseInsensitive) )
        m_type = t_if;
    else if ( !tagName.compare("else", Qt::CaseInsensitive) )
        m_type = t_else;
    else if ( !tagName.compare("elseif", Qt::CaseInsensitive) )
        m_type = t_elseif;
    else if ( !tagName.compare("endif", Qt::CaseInsensitive) )
        m_type = t_ifend;
    else if ( !tagName.compare("for", Qt::CaseInsensitive) )
        m_type = t_for;
    else if ( !tagName.compare("endfor", Qt::CaseInsensitive) )
        m_type = t_forend;
    else if ( !tagName.compare("foreach", Qt::CaseInsensitive) )
        m_type = t_foreach;
    else if ( !tagName.compare("endforeach", Qt::CaseInsensitive) )
        m_type = t_foreachend;
    else if ( !tagName.compare("f", Qt::CaseInsensitive) )
        m_type = t_func;
    else
        m_type = t_maths;
    return m_type;
}
QString WebTplTag::parseTagConditions()
{
    switch ( m_type )
    {
    case t_tpl:
        break;
    case t_if:
    case t_elseif: {
        WebTplVarMath maths( m_tpl );
        return maths.parseMaths( m_attr );
    }
    case t_for: {
        WebTplVarMath maths( m_tpl );
        return maths.parseMaths( m_attr );
    }
    case t_func:
        break;

    default:
        // no validation
        break;
    }
    return "1";
}

int WebTplTag::removeTag(QString &html)
{
    int linestart = html.lastIndexOf( '\n' ,pos() )+1;  // +1 um nach dem Return zu landen und dem -1 auszuweichen
    int linelng = html.indexOf( '\n' ,pos() ) - linestart;
    QString line = html.mid(linestart, linelng );
    //qDebug() << line << " crop " << line.trimmed() << " crop lng " << line.trimmed().length() << " tag lng " << lng();

    if ( line.trimmed().length() == (int)lng() )
    {
        m_tagPos = linestart;
        html.remove(linestart, linelng+1);
    }
    else
        html = html.remove( pos(), lng() );

    return pos();
}
bool WebTplTag::validate()
{
    m_validated = true;
    bool result = parseTagConditions().toDouble() != 0;
    m_true = result;
    traceTag( m_fullTag << " = " << m_true );
    return result;
}
QDebug operator<<(QDebug dbug, WebTplTag &t)
{
    QDebugStateSaver saver(dbug);
    dbug.noquote();
    dbug << t.tag() << " " << t.type() << " pos:" << t.pos() << " valid:" << t.stepInto() << " name:" << t.name() << " attr" << t.attr();
    return dbug;
}




WebTplVarMath::WebTplVarMath(WebTpl *tpl, QString &html, uint pos):
    m_tpl(tpl)
{

    html = parseVar(html, pos, html.indexOf("%>", pos) - pos);
}
WebTplVarMath::WebTplVarMath(WebTpl *tpl) :
    m_tpl(tpl)
{

}
WebTplVarMath::~WebTplVarMath()
{

}
QString WebTplVarMath::parseMaths(QString maths)
{
    traceMath2( "parseMaths" << maths);
    maths = maths.trimmed();

    if ( m_tpl->isset(maths) )
    {
        QString result = m_tpl->var(maths);
        if ( result.startsWith('"') && result.endsWith('"') )
            result = result.mid(1,result.length()-2);
        return result;
    }
    if ( m_tpl->isValidationCached(maths) )
        return m_tpl->getValidation(maths);

    QString res = solveBrakets(solveBrakets(maths));
    m_tpl->setValidation(maths, res);
    return res;
}
double WebTplVarMath::calcText(QString cond)
{
    traceMath3("calcText" << cond);
    cond = cond.trimmed();
    int pos = -1;

    pos = cond.indexOf("!");
    if ( pos != -1 )
        return calcText(cond.mid(pos+1, cond.indexOf(" ") )) == 0 ? 1 : 0;

    pos = cond.indexOf("&&");
    if ( pos != -1 )
        return calcText(cond.left(pos)) && calcText(cond.mid(pos+2));
    pos = cond.indexOf("||");
    if ( pos != -1 )
        return calcText(cond.left(pos)) || calcText(cond.mid(pos+2));

    pos = cond.indexOf("&");
    if ( pos != -1 )
        return (unsigned long long)calcText(cond.left(pos)) & (unsigned long long)calcText(cond.mid(pos+1));
    pos = cond.indexOf("|");
    if ( pos != -1 )
        return (unsigned long long)calcText(cond.left(pos)) | (unsigned long long)calcText(cond.mid(pos+1));


    //pos = cond.indexOf("%");  beist sich mit Variablen...
    pos = cond.indexOf("%%");
    if ( pos != -1 )
        return (int)calcText(cond.left(pos)) % (int)calcText(cond.mid(pos+2));

    pos = cond.indexOf("-");
    if ( pos != -1 )
        return calcText(cond.left(pos)) - calcText(cond.mid(pos+1));
    pos = cond.indexOf("+");
    if ( pos != -1 )
        return calcText(cond.left(pos)) + calcText(cond.mid(pos+1));

    pos = cond.indexOf("/");
    if ( pos != -1 )
    {
        double div = calcText(cond.mid(pos+1));
        if ( div == 0 )
            return 0;
        return calcText(cond.left(pos)) / div;
    }
    pos = cond.indexOf("*");
    if ( pos != -1 )
        return calcText(cond.left(pos)) * calcText(cond.mid(pos+1));
    pos = cond.indexOf("^");
    if ( pos != -1 )
        return pow(calcText(cond.left(pos)), calcText(cond.mid(pos+1)));

    if ( m_tpl->isset(cond) )
        cond = m_tpl->var(cond);
    pos = cond.indexOf("\"");
    if ( pos != -1 )
        return cond.mid(1, cond.size()-2).size();


    if ( cond.contains('.') )
        return cond.toFloat();
    if ( cond.contains(',') )
        return cond.replace(',','.').toFloat();
    if ( cond.startsWith("0x") )
        return cond.mid(2).toInt(nullptr,16);

    return cond.toLongLong();
}
double WebTplVarMath::solveConditions(QString cond)
{
    traceMath3( "solveConditions" << cond);
    int pos = -1;

    pos = cond.indexOf("!=");
    if ( pos != -1 )
    {
        if ( cond.contains('"') )  // hier werden texte verglichen
        {
            QString left = cond.left(pos).trimmed();
            QString right = cond.mid(pos+2).trimmed();

            if ( !left.contains('"') )
                left = m_tpl->var(left);
            if ( !right.contains('"') )
                right = m_tpl->var(right);
            return (left != right);
        }
        else
            return calcText(cond.left(pos)) != calcText(cond.mid(pos+2));
    }
    pos = cond.indexOf("==");
    if ( pos != -1 )
    {
        if ( cond.contains('"') )  // hier werden texte verglichen
        {
            QString left = cond.left(pos).trimmed();
            QString right = cond.mid(pos+2).trimmed();

            if ( !left.contains('"') )
                left = m_tpl->var(left);
            if ( !right.contains('"') )
                right = m_tpl->var(right);
            return (left == right);
        }
        else
            return calcText(cond.left(pos)) == calcText(cond.mid(pos+2));
    }

    pos = cond.indexOf("<=");
    if ( pos != -1 )
        return calcText(cond.left(pos)) <= calcText(cond.mid(pos+2));

    pos = cond.indexOf(">=");
    if ( pos != -1 )
        return calcText(cond.left(pos)) >= calcText(cond.mid(pos+2));

    pos = cond.indexOf("<");
    if ( pos != -1 )
        return calcText(cond.left(pos)) < calcText(cond.mid(pos+1));

    pos = cond.indexOf(">");
    if ( pos != -1 )
        return calcText(cond.left(pos)) > calcText(cond.mid(pos+1));

    return calcText(cond);
}
QString WebTplVarMath::solveBrakets(QString cond)
{
    traceMath3("solveBrakets" << cond);
    int a = cond.indexOf('(');
    int e = cond.indexOf(')');

    if ( a == e ) // keine Klammern mehr, dann kann der Hauptsectore validiert werden
        return QString::number( solveConditions(cond) );

    if ( (uint)e < (uint)a ) // zuerst eine ) dann kann der Sektor validiert werden und es geht evtl eine Stufe rauf.
        return QString::number( solveConditions(cond.left(e) ) ) + cond.mid(e+1); // validiere diesen Bereich

    //if ( (uint)a < (uint)e ) // zuerst eine ( dann geht es an der stelle eine stufe tiefer
    //{
    cond = cond.left(a) + solveBrakets( cond.mid(a+1) );
    if ( (uint)cond.indexOf('(') < (uint)cond.indexOf(')') ) // nochmal, da es noch eine Klammer hier in diesem teil gibt
        return solveBrakets( cond ); // selber nochmal im gleichen LVL aufrufen, um weitere Klammern zu finden
    return solveBrakets(cond);
    //}
}
QString WebTplVarMath::parseVar(QString html, uint pos, uint leng)
{
    m_fullTag = html.mid(pos, leng);
    m_var = m_fullTag.mid(2, lng() - 4);

    QString result = parseMaths( var() );
    traceMath( m_fullTag << " = " << result );

    return html.left(pos) + result + html.mid(pos+ leng);
}



bool WebTplCompressor::gzipCompress(QByteArray input, QByteArray &output, int level)
{
    // Prepare output
    output.clear();

    // Is there something to do?
    if(input.length())
    {
        // Declare vars
        int flush = 0;

        // Prepare deflater status
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;

        // Initialize deflater
        int ret = deflateInit2(&strm, qMax(-1, qMin(9, level)), Z_DEFLATED, GZIP_WINDOWS_BIT, 8, Z_DEFAULT_STRATEGY);

        if (ret != Z_OK)
            return(false);

        // Prepare output
        output.clear();

        // Extract pointer to input data
        char *input_data = input.data();
        int input_data_left = input.length();

        // Compress data until available
        do {
            // Determine current chunk size
            int chunk_size = qMin(GZIP_CHUNK_SIZE, input_data_left);

            // Set deflater references
            strm.next_in = (unsigned char*)input_data;
            strm.avail_in = chunk_size;

            // Update interval variables
            input_data += chunk_size;
            input_data_left -= chunk_size;

            // Determine if it is the last chunk
            flush = (input_data_left <= 0 ? Z_FINISH : Z_NO_FLUSH);

            // Deflate chunk and cumulate output
            do {

                // Declare vars
                char out[GZIP_CHUNK_SIZE];

                // Set deflater references
                strm.next_out = (unsigned char*)out;
                strm.avail_out = GZIP_CHUNK_SIZE;

                // Try to deflate chunk
                ret = deflate(&strm, flush);

                // Check errors
                if(ret == Z_STREAM_ERROR)
                {
                    // Clean-up
                    deflateEnd(&strm);

                    // Return
                    return(false);
                }

                // Determine compressed size
                int have = (GZIP_CHUNK_SIZE - strm.avail_out);

                // Cumulate result
                if(have > 0)
                    output.append((char*)out, have);

            } while (strm.avail_out == 0);

        } while (flush != Z_FINISH);

        // Clean-up
        (void)deflateEnd(&strm);

        // Return
        return(ret == Z_STREAM_END);
    }
    else
        return(true);
}

bool WebTplCompressor::gzipDecompress(QByteArray input, QByteArray &output)
{
    // Prepare output
    output.clear();

    // Is there something to do?
    if(input.length() > 0)
    {
        // Prepare inflater status
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;

        // Initialize inflater
        int ret = inflateInit2(&strm, GZIP_WINDOWS_BIT);

        if (ret != Z_OK)
            return(false);

        // Extract pointer to input data
        char *input_data = input.data();
        int input_data_left = input.length();

        // Decompress data until available
        do {
            // Determine current chunk size
            int chunk_size = qMin(GZIP_CHUNK_SIZE, input_data_left);

            // Check for termination
            if(chunk_size <= 0)
                break;

            // Set inflater references
            strm.next_in = (unsigned char*)input_data;
            strm.avail_in = chunk_size;

            // Update interval variables
            input_data += chunk_size;
            input_data_left -= chunk_size;

            // Inflate chunk and cumulate output
            do {

                // Declare vars
                char out[GZIP_CHUNK_SIZE];

                // Set inflater references
                strm.next_out = (unsigned char*)out;
                strm.avail_out = GZIP_CHUNK_SIZE;

                // Try to inflate chunk
                ret = inflate(&strm, Z_NO_FLUSH);

                switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                case Z_STREAM_ERROR:
                    // Clean-up
                    inflateEnd(&strm);

                    // Return
                    return(false);
                }

                // Determine decompressed size
                int have = (GZIP_CHUNK_SIZE - strm.avail_out);

                // Cumulate result
                if(have > 0)
                    output.append((char*)out, have);

            } while (strm.avail_out == 0);

        } while (ret != Z_STREAM_END);

        // Clean-up
        inflateEnd(&strm);

        // Return
        return (ret == Z_STREAM_END);
    }
    else
        return(true);
}



/**
 * @brief erzeugt aus dem erstellten Cookie eine zeile die als HTML headerline hinzugefügt werden kann
 * @return HTML Headerline für diese Cookie (ends with \r\n)
 */
QString WebTplCookie::getHeaderline()
{
    QString line;
    QTextStream l(&line);
    l << "Set-Cookie: "<< m_name << "=" << m_value;
    if ( m_path.size() )
        l << "; Path="<< m_path;
    else
        l << "; Path=/";
    if ( m_maxAge )
        l << "; Max-Age="<< m_maxAge;
    if ( m_domain.size() )
        l << "; Domain="<< m_domain;
    if ( m_secure )
        l << "; Secure";
    if ( m_httpOnly )
        l << "; HttpOnly";
    if ( !m_crossDomain )
        l << "; SameSite=Strict";
    l << "\r\n";

    return line;
}
/**
 * @brief übernimmt alle werte aus dem anderen Cookie
 * @param other Cookei das übernammen werden soll
 * @return return thisRef;
 */
WebTplCookie &WebTplCookie::operator = (WebTplCookie &other)
{
    m_name = other.m_name;
    m_value = other.m_value;
    m_path = other.m_path;
    m_domain = other.m_domain;
    m_maxAge = other.m_maxAge;
    m_httpOnly = other.m_httpOnly;
    m_secure = other.m_secure;
    m_crossDomain = other.m_crossDomain;
    return *this;
}




WebTplInterface::WebTplInterface(QObject *) :
    QSslSocket(),
    m_mobileClient(false)
{
    setProtocol(QSsl::SslV3);
    qsrand(QTime::currentTime().msec());
    m_keepAliveTimer.setSingleShot(true);
    m_keepAliveTimer.setInterval(5000);     // std timeout 21 Secunden for TCP connections

    connect(this, SIGNAL(readyRead()), this, SLOT(readData() ));
    connect(this, SIGNAL(disconnected()), this, SLOT(connectionClosed()));
    connect(this, SIGNAL(receveRequest()), this, SLOT(execHttpRequest()), Qt::QueuedConnection );
    connect(&m_keepAliveTimer, SIGNAL(timeout()), this, SLOT(keepAliveTimeout()));

    if( bytesAvailable() )
        readData();
}


bool WebTplInterface::setSocketDescriptor(qintptr socketDescriptor)
{
    bool ret = QSslSocket::setSocketDescriptor(socketDescriptor);
    //qDebug() << "MobileInterface created for: " << peerAddress().toString();
    return ret;
}
/**
 * @brief Schließt Verbindungstypen abhängig den Socket
 */
void WebTplInterface::close()
{
    if ( m_connection == "close" )
        QSslSocket::close();
    else
    {
        flush();        
        m_tpl.clearVar();
        m_tpl.clearValidateinCache();
        m_keepAliveTimer.start();
    }
}
/**
 * @brief erstellt einen HTML-Header der für die Sitzung angepasst ist.
 *
 * Task:
 * Setzt seinen Servernamen
 * Setzt ded ConnectionType : Keep-Alive / close
 * Schreibt die Cookies in den Header
 * Schreibt die Content-Length: in dezimal
 * @param status zustand des Requests "200 OK"
 * @param size  größe der antwort
 * @return
 */
QByteArray WebTplInterface::createHeader(QByteArray status, int size)
{
    QByteArray reply("HTTP/1.1 ");
    reply += status;
    reply += "\r\nServer: Halemba\r\n";
    if (m_connection.contains("close"))
        reply += "Connection: close\r\n";
    else
        reply += "Keep-Alive: timeout=5, max=1000\r\nConnection: Keep-Alive\r\n";
    foreach (WebTplCookie *cookie, m_newCookies)
        reply += cookie->getHeaderline();
    if (size)
        reply += "Content-Lenght:" + QByteArray::number(size) + "\r\n";
    return reply;
}
QByteArray WebTplInterface::createTextPackage(QString contentType, QByteArray replydata, QString lastModified, bool gzip)
{
    QByteArray reply( createHeader("200 OK") );
    if ( gzip )
        reply += "Content-Encoding: gzip\r\n";
    if ( lastModified.size() ){
        reply += "Last-Modified: " + lastModified.toLatin1() + "\r\n";
        QLocale locale  = QLocale(QLocale::C);
        reply += "Expires: " + locale.toString(QDateTime::currentDateTimeUtc().addYears(2), "ddd, dd MMM yyyy hh:mm:ss t").toLatin1() + "\r\n";
        reply += "Cache-Control: public, max-age=604800\r\n";
    }
    reply += "Transfer-Encoding: chunked\r\nContent-Type: " + contentType.toLatin1() + "\r\n\r\n";
    QString size = QString("%1\r\n").arg( replydata.size(),0,16 );

    reply += size.toLatin1();
    reply += replydata;
    reply.append("\r\n0\r\n\r\n");
    return reply;
}
QByteArray WebTplInterface::createBinaryPackage(QByteArray replydata)
{
    QByteArray reply = createHeader( "200 OK", replydata.size());
    reply += "content-disposition: attachment; filename=\"update.spk\"\r\nContent-Type: application/octet-stream\r\n\r\n";
    // ?? reply += "Transfer-Encoding: chunked\r\n";
    reply += replydata;
    return reply;
}
QByteArray WebTplInterface::createHtmlPackage( QByteArray replydata )
{
    QByteArray reply( createHeader( "200 OK", replydata.size())); // no blank. SONST GEHTS NICHT!! alles zusammen schreiben!
    reply += "Content-Type: text/html; charset=UTF-8\r\n";
    reply += "Transfer-Encoding: chunked\r\n";
    reply += "\r\n";
    reply += QByteArray::number(replydata.size(), 16) +"\r\n";
    reply += replydata;
    reply.append("\r\n0\r\n\r\n");
    return reply;
}
QByteArray WebTplInterface::createErrorPackage(QByteArray status, QString error)
{
    QByteArray err = error.toLatin1();
    QByteArray reply = createHeader( status, err.size());
    reply += "Content-Type: text/html; charset=UTF-8\r\n";
    reply += "Transfer-Encoding: chunked\r\n";
    reply += "\r\n";
    reply += QByteArray::number(err.size(), 16) +"\r\n";
    reply += err;
    reply.append("\r\n0\r\n\r\n");
    return reply;
}

QByteArray WebTplInterface::gZipData(QByteArray data) const
{
    QByteArray compressedData = qCompress(data);
    compressedData.remove(0, 6);    //  Strip the first six bytes (a 4-byte length put on by qCompress and a 2-byte zlib header)
    compressedData.chop(4);         // and the last four bytes (a zlib integrity check).

    QByteArray header;
    QDataStream ds1(&header, QIODevice::WriteOnly);
    ds1 << quint16(0x1f8b) << quint16(0x0800) << quint16(0x0000) << quint16(0x0000) << quint16(0x000b); // Prepend a generic 10-byte gzip header (see RFC 1952),

    QByteArray footer;
    QDataStream ds2(&footer, QIODevice::WriteOnly);
    ds2.setByteOrder(QDataStream::LittleEndian);
    ds2 << crc32buf(data) << quint32(data.size());    // Append a four-byte CRC-32 of the uncompressed data and append 4 bytes uncompressed input size modulo 2^32

    return header + compressedData + footer;
}

const quint32 crc_32_tab[] = { /* CRC polynomial 0xedb88320 */
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};
quint32 WebTplInterface::crc32buf(const QByteArray &data) const
{
    return ~std::accumulate( data.begin(), data.end(), quint32(0xFFFFFFFF), [](quint32 oldcrc32, char buf){
        return (crc_32_tab[((oldcrc32) ^ ((quint8)buf)) & 0xff] ^ ((oldcrc32) >> 8));
    });
}

/**
 * @brief Slot der aufgerufen wird, wenn die Verbindung geschlossen wurde.
 *
 * Gibt eine qDebug info aus und löscht den socket über die mainloop
 */
void WebTplInterface::connectionClosed()
{    
    deleteLater();
}

/**
 * @brief slot der emitiert wird, wenn Daten über den Socket herrein kommen.
 *
 * ruft direkt WebTplInterface::parseRequest() auf
 */
void WebTplInterface::readData()
{
    // receve data ----------------------------------------------------------------------------------
    m_request.clear();
    while( bytesAvailable() )
        m_request.append( readAll() );


    if ( m_request.isEmpty() )
    { LogD << "request Empty"; m_request.clear(); close(); return; }

    parseRequest();
}
/**
 * @brief verarbeitet den HTML-Request
 *
 * Headers die größer als 10KB sind, werden nicht unterstützt
 *
 * Output:
 * * command
 * * PostData
 * * m_hostAddress
 * * m_connection
 * * m_clientAgent
 * * m_mobileClient
 * * Cookies
 */
void WebTplInterface::parseRequest()
{
    // parse data ----------------------------------------------------------------------------------
    QStringList httpRequest = QString(m_request.left(10240)).split("\r\n");
    if ( httpRequest.size() < 2 )
    {
        close();
        return;
    }
    QStringList requestInfo = httpRequest[0].split(" ");
    if ( requestInfo.size() != 3 || (requestInfo[0] != "POST" && requestInfo[0] != "GET") )
    {
        close();
        return;
    }

    m_command = requestInfo[1];

    QString contType;
    int contLng = 0;
    foreach (QString line, httpRequest) {
        if (  line.isEmpty() )
            break;
        if ( line.startsWith("Content-Type:") )
            contType = line.mid(14);
        else if ( line.startsWith("Content-Length:") )
            contLng = line.mid(16).toInt();
        else if ( line.startsWith("Host:") )
            m_hostAddress = line.mid(6);
        else if ( line.startsWith("Connection:") )
            m_connection = line.mid(12);
        else if ( line.startsWith("User-Agent:") )
            m_clientAgent = line.mid(12);
        else if ( line.startsWith("Cookie:") )
            parseCookies( line.mid(8) );
        else if ( line.startsWith("accept-encoding", Qt::CaseInsensitive) )
            m_acceptGZip = line.contains("gzip");
        else if ( line.startsWith("Accept-Language:") )
        {
            QString langStr = line.mid(17);
            if ( langStr.size() )
                m_lang = langStr.split(',');
            else
                m_lang << "de-DE";
        }
    }
    m_mobileClient = m_clientAgent.contains("Android") || m_clientAgent.contains("iPhone") || m_clientAgent.contains("Mobile");


    if ( requestInfo[0] == "POST")
    {
        contLng += m_request.indexOf("\r\n\r\n")+4; // request is Content + header (header ends with \r\n\r\n)

        if ( m_request.size() < contLng)
        {
            m_request.reserve(contLng+1);
            while ( m_request.size() < contLng && state() == ConnectedState )
            {
                waitForReadyRead(50);
                if ( bytesAvailable() )
                    m_request.append( readAll() );
            }
        }

        if ( contType.contains("multipart/form-data") )
            m_postData = extractMultipartFormData(contType, m_request);
        else
            m_postData = extractWWWForm(QString(m_request).split("\r\n").last());
    }

    emit receveRequest();
}
/**
 * @brief Slot, der aufgerufen wird, wenn die Session abläuft
 */
void WebTplInterface::keepAliveTimeout()
{
    QSslSocket::close();
}
/**
 * @brief Setter für die Cookieinformationen aus dem Header des HTML-packets
 *
 * Daten werden in m_cookieData gespeichert.
 * Es werden nur Daten interpretiert, die einen Wert besizten z.B. "name=Alex" is ok, "readOnly" is filtered aut
 * @param cookiedata    String aus dem HTML-Header
 */
void WebTplInterface::parseCookies(QString cookiedata)
{
    QStringList infos = cookiedata.split(';');
    foreach (QString info, infos) {
        QStringList datas = info.split('=');
        if ( datas.size() != 2 )
            continue;
        m_cookieData[datas.first().trimmed()] = qVariantFromValue( escapeHTML( datas.last() ) );
    }
}

QMap<QString, QVariant> WebTplInterface::extractWWWForm(QString post)
{
    QMap<QString, QVariant> postData;
    if ( post.size() )
    {
        QStringList data = post.split("&");
        foreach ( QString info, data)
        {
            QString data = info.section('=',1,1);
            data = data.replace("+"," ");
            data = escapeHTML(data);

            QString name = info.section('=',0,0);
            if ( !postData.contains(name) )
                postData[name] = qVariantFromValue( data );
            else
            {
                int arraySize = 0;
                if ( !postData.contains(name+".size") )
                {
                    QString data = postData[name].toString();
                    postData[name] = QString("array");
                    postData[name+"[0]"] = data;
                    postData[name+".size"] = 1;
                }

                arraySize = postData[name+".size"].toInt() + 1;
                postData[name+".size"] = arraySize;
                postData[name+"["+QString::number(arraySize-1)+"]"] = qVariantFromValue( data );
            }
        }
    }
    return postData;
}
QMap<QString, QVariant> WebTplInterface::extractMultipartFormData(QString header, QByteArray &data)
{
    QMap<QString, QVariant> postData;
    if ( header.indexOf("=") == -1)
        return postData;
    QString bounding = "--" + header.split('=')[1];

    int nextBound = data.indexOf(bounding.toLatin1());
    qDebug() << header << "/r/n" << nextBound;

    while( nextBound != -1)
    {
        QString name;
        QString filename;
        QString line;

        int blockStart = nextBound + bounding.size()+2; // + \r\n

        int nextLineStart = blockStart;
        int linelng = 0;
        do
        {
            linelng = data.indexOf("\r\n", nextLineStart) - nextLineStart;

            line = data.mid( nextLineStart, linelng );
            nextLineStart += linelng + 2; // + /r/n

            if ( line.startsWith( "Content-Disposition:" ) )
            {
                QStringList disposition = line.split("; ");
                foreach (QString dis, disposition)
                {
                    if ( dis.startsWith("name=") )
                        name = dis.mid(6, dis.size()-7); // skip \"
                    else if ( dis.startsWith("filename=") )
                        filename = dis.mid(10, dis.size()-11); // skip \"
                }
                if ( filename.size() )
                    postData[name+"_filename"] = filename;
            }
            else if ( line.startsWith( "Content-Type:" ) )
            {
                QStringList type = line.split(" ");
                if ( type.size() > 1)
                    postData[name+"_filetype"] = type[1];
            }

        }
        while ( linelng > 5 );


        int dataStart = nextLineStart; //
        nextBound = data.indexOf(bounding, nextBound+1);

        if ( !postData.contains(name) )
        {
            int datasize = nextBound-dataStart-2;
            postData[name] = QByteArray::fromRawData(data.data()+dataStart, datasize);
        }
        else
        {
            int arraySize = 0;
            if ( !postData.contains(name+".size") )
            {
                QString data = postData[name].toString();
                postData[name] = QString("array");
                postData[name+"[0]"] = data;
                postData[name+".size"] = 1;
            }

            arraySize = postData[name+".size"].toInt() + 1;
            postData[name+".size"] = arraySize;
            postData[name+"["+QString::number(arraySize-1)+"]"] = data.mid(dataStart, nextBound-dataStart-2);
        }
    }
    return postData;
}

/**
 * @brief getter für die gesendeten Postdaten einer Anfrage.
 *
 * Sind die Daten nur als ByteArray convertierbar, werden die Daten als QByteArray zu QString convertiert
 * @param key Key, dessen Wert zurück gegeben werden soll
 * @return Ist ein Eintrag nicht vorhanden, wird "" zurück gegeben, sond der wert als QString
 */
QString WebTplInterface::post(QString key)
{
    if ( m_postData.contains(key) )
    {
        if ( m_postData[key].canConvert(QVariant::ByteArray) )
            return m_postData[key].toByteArray();
        else
            return m_postData[key].toString();
    }
    return "";
}
/**
 * @brief decodiert einen HTML-encoded String.
 *
 * z.B.: "Hallo%20Welt%21" wird zu "Hallo Welt!"
 * @param data  Zeichenkette, die decodiert werden soll
 * @return  Decodierte Zeichenkette
 */
QString WebTplInterface::escapeHTML(QString data)
{
    data = data.replace("%5E","^");
    data = data.replace("%20"," ");
    data = data.replace("%21","!");
    data = data.replace("%22","\"");
    data = data.replace("%23","#");
    data = data.replace("%24","$");
    data = data.replace("%25","%");
    data = data.replace("%26","&");
    data = data.replace("%27","'");
    data = data.replace("%28","(");
    data = data.replace("%29",")");
    data = data.replace("%2B","+");
    data = data.replace("%2F","/");
    data = data.replace("%3D","=");
    data = data.replace("%3F","?");
    data = data.replace("%40","@");

    data = data.replace("%C2%B0","°");
    data = data.replace("%C2%A7","§");
    data = data.replace("%C3%BC","ü");
    data = data.replace("%C3%9F","ß");
    data = data.replace("%C3%B6","ö");
    data = data.replace("%C3%A4","ä");
    data = data.replace("%C3%9C","Ü");
    data = data.replace("%C3%96","Ö");
    data = data.replace("%C3%84","Ä");

    data = data.replace("%3A",":");
    data = data.replace("%2C",",");
    data = data.replace("%3B",";");
    data = data.replace("%7E","~");
    data = data.replace("%3C","<");
    data = data.replace("%3E",">");
    data = data.replace("%7C","|");
    return data;
}

/**
 * @brief Erzeugt einen "escapeden Text" der JSON compatibel ist
 *
 * 1. Convertierung mit QString::escapeHTML
 * 2. \ mit  \\ ersetzen
 * @param text Eingabetext der convertiert werden soll
 * @return
 */
QString WebTplInterface::toJSON(QString text)
{
    text = text.toHtmlEscaped();
    text = text.replace("\\", "\\\\");
    return text;
}
/**
 * @brief weist die WebTpl engine an, die Variablen für eine JSON antwort zu convertieren
 */
void WebTplInterface::convertToJSON()
{
    m_tpl.convertAllVarToJSON();
}
/**
 * @brief zerlegt eine parameterliste und gibt diese als map zurück.
 *
 * Das soll den Zugriff auf Dataqueries erleichtern.
 * Unterstützte werte:
 *  key(seperator[:])value  ==> map[key] = value
 *  key                     ==> map[key] = "true"
 *
 * @param parameter     Parameterliste z.B.: [ "username:name", "passwort:1c6f2ea3c6d78fc4064", "sonst%20noch%20was:lasduhzflabeu", "readonly"]
 * @param seperatore    String an dem die Parameter getrennt werden sollen
 * @return map mit den aufgelösten parametern.
 */
QMap<QString, QString> WebTplInterface::encodeParameterlist(QStringList parameter, QString seperator)
{
    QMap<QString, QString> map;
    foreach (QString entry, parameter)
    {
        QStringList parts = entry.split(seperator);
        if ( parts.count() > 2)
        {
            int sep = entry.indexOf(seperator);
            map[ entry.left(sep) ] = entry.mid(sep+1);
        }
        else if ( parts.count() == 2)
            map[ parts[0] ] = parts[1];
        else if (parts.count() == 1)
            map[ parts[0] ] = "true";
    }
    return map;
}
/**
 * @brief Erzeugt ein neues Cookie, das in der Antwort mit gesendet wird.
 * @param name  Name des Cookies
 * @param value Wert des Cookies
 * @return Das erzeugt Cookie wird zurück gegeben um es, wenn nötig, weiter anzupassen
 */
WebTplCookie *WebTplInterface::createCookie(QString name, QString value)
{
    WebTplCookie *c = new WebTplCookie(name, value);
    m_newCookies[name] = c;
    return c;
}
/**
 * @brief Gibt den Metatyp einer Datei anhand ihrer Endung zurück.
 *
 * Unterstützte Typen: .css, .js, .png, .jpg, .gif
 *
 * sonst wird "" zurück gegeben
 *
 * @param meta  Dateiname / pfard
 * @return Erkannter metatype z.B.: "text/css" oder ""
 */
QByteArray WebTplInterface::createMetatype(QString meta)
{
    if (meta.endsWith(".css", Qt::CaseInsensitive))
        return "text/css";
    else if (meta.endsWith(".js", Qt::CaseInsensitive))
        return "application/javascript";
    else if (meta.endsWith(".png", Qt::CaseInsensitive))
        return "image/png";
    else if (meta.endsWith(".jpg", Qt::CaseInsensitive))
        return "image/jpeg";
    else if (meta.endsWith(".gif", Qt::CaseInsensitive))
        return "image/gif";
    else if (meta.endsWith(".appcache", Qt::CaseInsensitive))
        return "text/cache-manifest";
    return "";
}

} // namespace UpdateServer

