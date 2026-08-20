#ifndef FILEMGMT_H
#define FILEMGMT_H

/* Stores a document reference (not the binary file itself) against an
   employee_id — e.g. "1|resume.pdf|/docs/emp1_resume.pdf". Mirrors the
   "Document Upload" scope under Employee Management in the design docs.

   employee_id: pass the caller's own ID from the Employee menu (self-serve
   upload, no prompt); pass 0 from the Admin/HR menu to be prompted for
   which employee to upload on behalf of. */
void upload_document(int employee_id);
void view_documents(int employee_id); /* 0 = view all */
void delete_document(void);

/* Cascade helper used by employee.c when deleting an employee — also
   removes the copied files from data/docs/, not just the index rows. */
void delete_documents_for_employee(int employee_id);

#endif
