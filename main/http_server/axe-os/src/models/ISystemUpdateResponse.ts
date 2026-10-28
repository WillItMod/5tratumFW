export interface SystemUpdateResponse {
  status?: string;
  redirect?: {url: string; delay?: number; message?: string};
}
