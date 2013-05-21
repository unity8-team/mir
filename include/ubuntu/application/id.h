#ifndef UBUNTU_APPLICATION_ID_H_
#define UBUNTU_APPLICATION_ID_H_

#ifdef __cplusplus
extern "C" {
#endif

    typedef void UApplicationId;
    
    UApplicationId*
    u_application_id_new_from_stringn(
    	const char *string, 
    	size_t size);
    
    void
    u_application_id_destroy(UApplicationId *id);
    
    int
    u_application_id_compare(
    	UApplicationId *lhs,
    	UApplicationId *rhs);
       
#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_ID_H_ */
