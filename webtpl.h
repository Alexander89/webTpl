#ifndef UPDATESERVER_WEBTPL_H
#define UPDATESERVER_WEBTPL_H

#include <QMap>
#include <QObject>
#include <QSslSocket>
#include <QTimer>

namespace WebTpl {

class WebTpl;
class WebTplTag;

struct ForeachInfo {
    ForeachInfo() {
        m_disableValidation = true;
    }
    ForeachInfo(const ForeachInfo& o) {
        m_tag = o.m_tag;
        m_key = o.m_key;
        m_keys = o.m_keys;
        m_value = o.m_value;
        m_values = o.m_values;
        m_disableValidation = o.m_disableValidation;
    }
    WebTplTag *m_tag;
    QString m_key;
    QStringList m_keys;
    QString m_value;
    QStringList m_values;
    bool m_disableValidation;
    bool valDisabled() {return m_disableValidation;}
};
struct ForInfo {
    ForInfo() {
        m_disableValidation = true;
    }
    ForInfo(const ForInfo& o) {
        m_tag = o.m_tag;
        m_key = o.m_key;
        m_start = o.m_start;
        m_value = o.m_value;
        m_end = o.m_end;
        m_step = o.m_step;
        m_disableValidation = o.m_disableValidation;
    }
    WebTplTag *m_tag;
    QString m_key;
    int m_start;
    int m_value;
    int m_end;
    int m_step;
    bool m_disableValidation;
    bool valDisabled() {return m_disableValidation;}
};
struct IfInfo {
    IfInfo() { reset(); }
    IfInfo(const IfInfo& o) { m_removeStart = o.m_removeStart; m_hasValidPart = o.m_hasValidPart; }
    int  m_removeStart;
    bool m_hasValidPart;
    bool disabledSection() { return m_removeStart != -1; }
    void reset() { m_removeStart = -1; m_hasValidPart = false; }
};

enum TagType {
    t_undef,
    t_tpl,
    t_if,
    t_else,
    t_elseif,
    t_ifend,
    t_for,
    t_forend,
    t_foreach,
    t_foreachend,

    t_maths,
    t_func,
};

class WebTplCookie
{
public:
    WebTplCookie( WebTplCookie &other )
    { *this = other; }

    explicit WebTplCookie( QString name, QString value) :
        m_name(name),
        m_value(value),
        m_maxAge(0),
        m_httpOnly(false),
        m_secure(false),
        m_crossDomain(false)
    {}

    WebTplCookie* setPath( QString path ) { m_path = path; return this; }
    WebTplCookie* setDomain( QString domain ) { m_domain = domain; return this; }
    WebTplCookie* setMaxAge_Days( int age ){return setMaxAge_Hours(age*24);}
    WebTplCookie* setMaxAge_Hours( int age ){return setMaxAge_Hours(age*60);}
    WebTplCookie* setMaxAge_Min( int age ){return setMaxAge_Min(age*60);}
    WebTplCookie* setMaxAge( int age ){ m_maxAge = age; return this;}
    WebTplCookie* httpOnly(){m_httpOnly = true; return this;}
    WebTplCookie* secureOnly(){m_secure = true; return this;}
    WebTplCookie* enableCrossDomain(){m_crossDomain = true; return this;}

    QString getHeaderline();
    WebTplCookie &operator =(WebTplCookie &other);

private:
    QString m_name;         ///< can be anything but control characters (CTLs) or spaces and tabs. It also must not contain a separator character like the following: ( ) < > @ , ; : \ " /  [ ] ? = { }.
    QString m_value;        ///< can optionally be set in double quotes and any US-ASCII characters excluding CTLs, whitespace, double quotes, comma, semicolon, and backslash are allowed.
    QString m_path;         ///< Indicates a URL path that must exist in the requested resource before sending the Cookie header (/ == ALL )
    QString m_domain;       ///< Specifies those hosts to which the cookie will be sent. If not specified, defaults to the host portion of the current document location (but not including subdomains).
    int m_maxAge;           ///< number of secend til Cookie is expired // no value ==> SessionCookey
    bool m_httpOnly;        ///< not send on Ajax requets
    bool m_secure;          ///< only send on https (request from client)
    bool m_crossDomain;     ///< cookie will not be send, when a cross platform call is done (link to extern side)
};
class WebTplTag
{
public:
    WebTplTag(WebTpl *tpl, QString tag, uint pos );
    ~WebTplTag();

    QString tag()  { return m_fullTag; }
    uint    lng()  { return m_fullTag.size();}
    uint    pos()  { return m_tagPos;}
    QString name() { return m_tagname; }
    QString attr() { return m_attr; }
    TagType type() { return m_type; }

    bool  stepInto() { if (m_validated) return m_true; else return validate();}
    int removeTag(QString &html);

private:
    void parseTagInfo( QString tag );
    TagType parseTagTpl( QString tagName );
    QString parseTagConditions( );
    bool validate();

    friend QDebug operator<<(QDebug dbug, WebTplTag &t);

private:
    WebTpl *m_tpl;
    QString m_tagname;      ///< Name nach <$ (<$IF (Perms = U_L_FREE)$> --> IF )
    TagType m_type;         ///< tpe, nachdem der tag behandelt werden soll (m_tagname is case Insensetiv)
    QString m_fullTag;      ///< kompletter tag
    QString m_attr;         ///< atribute die nach dem tagnamen stehen (<$IF (Perms = U_L_FREE) $> --> (Perms = U_L_FREE) )

    uint    m_tagPos;       ///< pos , wo der tag in der tpl-Datei gefunden wurde

    bool    m_validated;
    bool    m_true;
};
class WebTplVarMath
{

public:
    WebTplVarMath( WebTpl *tpl, QString &html, uint pos );
    WebTplVarMath( WebTpl *tpl);
    ~WebTplVarMath();

    QString tag()  { return m_fullTag; }
    uint    lng()  { return m_fullTag.size();}
    uint    pos()  { return m_tagPos;}
    QString var()  { return m_var; }

    QString parseVar(QString html, uint pos , uint leng);
    QString parseMaths(QString maths);
private:

    double calcText(QString cond);
    double solveConditions(QString cond);
    QString solveBrakets( QString cond );


private:
    QString m_var;          ///< Name nach <$ $> (<$perms>)
    QString m_fullTag;      ///< kompletter tag
    uint    m_tagPos;       ///< pos , wo der tag in der tpl-Datei gefunden wurde

    WebTpl  *m_tpl;
    QString m_formel;
};

#define GZIP_WINDOWS_BIT 15 + 16
#define GZIP_CHUNK_SIZE 32 * 1024

class WebTplCompressor
{
public:
    static bool gzipCompress(QByteArray input, QByteArray &output, int level = -1);
    static bool gzipDecompress(QByteArray input, QByteArray &output);
};


class WebTpl : public QObject
{
    Q_OBJECT

public:
    explicit WebTpl(QObject *parent = 0);

    QByteArray createHTML(QString file);

    WebTpl *clearVar() {m_var.clear(); return this;}
    WebTpl *clearValidateinCache() {m_validationCache.clear(); return this;}
    void publishVar(QString name, const QVariant value);
    WebTplCookie *createCookie(QString name, QString value);
    bool isset(QString varname) const;
    QString var(QString varname) const;
    QList<QPair<QString,QString>> getList(QString varname) const;
    QString dumpVar() const;
    void convertAllVarToJSON();

    QString convertColor(uint color);

    bool isValidationCached(QString condition);
    QString getValidation(QString condition);
    void setValidation(QString condition, QString value);

private:
    QString applyTplSyntax(QString html, uint start = 0, uint line = 0);

    QString applyIF(QString html, WebTplTag &tag, int &nextTag);
    QString applyFor(QString html, WebTplTag &tag, int &nextTag);
    QString applyForeach(QString html, WebTplTag &tag, int &nextTag);
    QString applyFunc(QString html, WebTplTag &tag, int &nextTag);

    QString loadTplFile(QString path);
    QString setVariables(QString html);

    int ifLVL(){ return m_ifInfo.size()-1; }
    IfInfo &getIfLVL(){ return m_ifInfo[ifLVL()]; }
    IfInfo &getLastIfLVL(){ return m_ifInfo[ifLVL()-1]; }

    int foreachLVL(){ return m_foreachInfo.size()-1; }
    ForeachInfo &getForeachLVL(){ return m_foreachInfo[foreachLVL()]; }
    ForeachInfo &getLastForeachLVL(){ return m_foreachInfo[foreachLVL()-1]; }

    int forLVL(){ return m_forInfo.size()-1; }
    ForInfo &getForLVL(){ return m_forInfo[forLVL()]; }
    ForInfo &getLastForLVL(){ return m_forInfo[forLVL()-1]; }
signals:

public slots:

private:
    QList<IfInfo>           m_ifInfo;
    QList<ForeachInfo>      m_foreachInfo;
    QList<ForInfo>          m_forInfo;


    QMap<QString, QString>  m_var;
    QMap<QString, QString>  m_validationCache;
};


class WebTplInterface : public QSslSocket
{
    Q_OBJECT

    friend class WebTpl;
public:
    explicit WebTplInterface( QObject *parent = nullptr);
    bool setSocketDescriptor(qintptr socketDescriptor);

    virtual void close();

protected:
    QByteArray createHeader(QByteArray status, int size = 0);
    QByteArray createTextPackage(QString contentType, QByteArray replydata, QString lastModified = QString(), bool gzip = false);
    QByteArray createBinaryPackage(QByteArray replydata);
    QByteArray createHtmlPackage( QByteArray replydata );
    QByteArray createErrorPackage(QByteArray status, QString error);
public:
    QByteArray gZipData( QByteArray data ) const;
    quint32 crc32buf(const QByteArray& data) const ;

private slots:
    void connectionClosed();
    void readData();
    void parseRequest();
    void keepAliveTimeout();
    virtual void execHttpRequest() = 0;

private:
    void parseCookies(QString cookiedata);
    QMap<QString, QVariant> extractWWWForm(QString post);
    QMap<QString, QVariant> extractMultipartFormData(QString header, QByteArray &data);

signals:
    void receveRequest();

protected:
    QString post(QString key);
    QByteArray createMetatype( QString meta);
    QString escapeHTML(QString data);
    static QString toJSON(QString text);
    void convertToJSON();
    QMap<QString, QString> encodeParameterlist(QStringList parameter, QString seperator =":");
    WebTplCookie *createCookie(QString name, QString value);

protected:
    QByteArray                      m_request;
    QString                         m_command;
    QMap<QString, QVariant>         m_postData;
    QMap<QString, QVariant>         m_cookieData;

    QString                         m_hostAddress;
    QString                         m_connection;
    QString                         m_clientAgent;
    bool                            m_acceptGZip;
    QStringList                     m_lang;
    bool                            m_mobileClient;

    QTimer                          m_keepAliveTimer;

    WebTpl                          m_tpl;
    QMap<QString, WebTplCookie*>    m_newCookies;
};
#define PUBLISH_M(x) m_tpl.publishVar(QString(#x).mid(2), qVariantFromValue(x) )
#define PUBLISH(x) m_tpl.publishVar(QString(#x), qVariantFromValue(x) )
#define PUBLISH_LL(x) m_tpl.publishVar(QString(#x), qVariantFromValue((unsigned long long)x) )

QDebug operator<<(QDebug dbug, WebTplTag &t);
} // namespace UpdateServer

#endif // UPDATESERVER_WEBTPL_H
