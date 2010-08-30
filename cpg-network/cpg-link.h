#ifndef __CPG_LINK_H__
#define __CPG_LINK_H__

#include <cpg-network/cpg-object.h>
#include <cpg-network/cpg-expression.h>
#include <cpg-network/cpg-link-action.h>

G_BEGIN_DECLS

#define CPG_TYPE_LINK            (cpg_link_get_type ())
#define CPG_LINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CPG_TYPE_LINK, CpgLink))
#define CPG_LINK_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), CPG_TYPE_LINK, CpgLink const))
#define CPG_LINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CPG_TYPE_LINK, CpgLinkClass))
#define CPG_IS_LINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CPG_TYPE_LINK))
#define CPG_IS_LINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CPG_TYPE_LINK))
#define CPG_LINK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CPG_TYPE_LINK, CpgLinkClass))

typedef struct _CpgLink        CpgLink;
typedef struct _CpgLinkClass   CpgLinkClass;
typedef struct _CpgLinkPrivate CpgLinkPrivate;

struct _CpgLink
{
	/*< private >*/
	CpgObject parent;

	CpgLinkPrivate *priv;
};

struct _CpgLinkClass
{
	/*< private >*/
	CpgObjectClass parent_class;

	void (*action_added)   (CpgLink *link, CpgLinkAction *action);
	void (*action_removed) (CpgLink *link, CpgLinkAction *action);
};

GType          cpg_link_get_type              (void) G_GNUC_CONST;

CpgLink       *cpg_link_new                   (const gchar   *id,
                                               CpgObject     *from,
                                               CpgObject     *to);

CpgObject     *cpg_link_get_from              (CpgLink       *link);
CpgObject     *cpg_link_get_to                (CpgLink       *link);

gboolean       cpg_link_add_action            (CpgLink       *link,
                                               CpgLinkAction *action);

gboolean       cpg_link_remove_action         (CpgLink       *link,
                                               CpgLinkAction *action);

const GSList  *cpg_link_get_actions           (CpgLink       *link);
CpgLinkAction *cpg_link_get_action            (CpgLink       *link,
                                               const gchar   *target);
void           cpg_link_attach                (CpgLink       *link,
                                               CpgObject     *from,
                                               CpgObject     *to);

CpgLink       *cpg_link_get_action_template   (CpgLink       *link,
                                               CpgLinkAction *action,
                                               gboolean       match_full);

G_END_DECLS

#endif /* __CPG_LINK_H__ */
