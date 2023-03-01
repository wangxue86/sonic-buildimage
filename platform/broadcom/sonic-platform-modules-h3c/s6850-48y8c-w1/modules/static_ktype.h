#ifndef __STATIC_KTYPE_H
#define __STATIC_KTYPE_H

/* default kobject attribute operations */
static ssize_t static_kobj_attr_show (struct kobject *kobj, struct attribute *attr,
                                      char *buf)
{
    struct kobj_attribute *kattr;
    ssize_t ret = -EIO;
 
    kattr = container_of(attr, struct kobj_attribute, attr);
    if (kattr->show)
        ret = kattr->show(kobj, kattr, buf);
    
    return ret;
}
 
static ssize_t static_kobj_attr_store(struct kobject *kobj, struct attribute *attr,
                                      const char *buf, size_t count)
{
    struct kobj_attribute *kattr;
    ssize_t ret = -EIO;
 
    kattr = container_of(attr, struct kobj_attribute, attr);
    if (kattr->store)
        ret = kattr->store(kobj, kattr, buf, count);
    
    return ret;
}
 
const struct sysfs_ops kobj_sysfs_ops = {
    .show   = static_kobj_attr_show,
    .store  = static_kobj_attr_store,
};
 
static void static_kobj_release(struct kobject *kobj)
{
    pr_debug("no need to free static kobject: (%p): %s\n", kobj, __func__);
}

static struct kobj_type static_kobj_ktype = {
    .release    = static_kobj_release,
    .sysfs_ops  = &kobj_sysfs_ops,
};
#endif
