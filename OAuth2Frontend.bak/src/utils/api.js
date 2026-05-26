import axios from 'axios';
import router from '../router';

// 创建 axios 实例
const apiClient = axios.create({
  // 根据环境变量设置基础 URL，如果有的话，否则默认为当前域名
  baseURL: import.meta.env.VITE_API_BASE_URL || '',
  timeout: 10000,
});

// 正在刷新 Token 的标志
let isRefreshing = false;
// 存储因为刷新 Token 而被挂起的请求队列
let failedQueue = [];

// 将被挂起的请求重新发起
const processQueue = (error, token = null) => {
  failedQueue.forEach(prom => {
    if (error) {
      prom.reject(error);
    } else {
      prom.resolve(token);
    }
  });
  
  failedQueue = [];
};

// 请求拦截器
apiClient.interceptors.request.use(
  config => {
    // 从 localStorage 获取 Access Token
    const token = localStorage.getItem('access_token');
    
    if (token) {
      // 在请求头中添加 Authorization: Bearer <token>
      config.headers['Authorization'] = `Bearer ${token}`;
    }
    
    return config;
  },
  error => {
    return Promise.reject(error);
  }
);

// 响应拦截器
apiClient.interceptors.response.use(
  response => {
    return response;
  },
  async error => {
    const originalRequest = error.config;

    // 检查是否是 401 未授权错误，且未重试过
    if (error.response?.status === 401 && !originalRequest._retry) {
      
      // 对于登录、续期或获取 Token 自身的端点，不进行拦截重试
      if (originalRequest.url.includes('/oauth2/token') || originalRequest.url.includes('/oauth2/authorize')) {
        return Promise.reject(error);
      }

      if (isRefreshing) {
        // 如果正在刷新 Token，将当前请求加入队列，返回一个 Promise
        return new Promise(function(resolve, reject) {
          failedQueue.push({ resolve, reject });
        }).then(token => {
          // Token 刷新成功，重新发起该请求
          originalRequest.headers['Authorization'] = `Bearer ${token}`;
          return apiClient(originalRequest);
        }).catch(err => {
          return Promise.reject(err);
        });
      }

      originalRequest._retry = true;
      isRefreshing = true;

      const refreshToken = localStorage.getItem('refresh_token');
      
      if (!refreshToken) {
        // 没有 Refresh Token，只能让用户重新登录
        isRefreshing = false;
        clearTokens();
        router.push('/');
        return Promise.reject(error);
      }

      try {
        // 发起刷新 Token 请求
        const response = await axios.post(
          (import.meta.env.VITE_API_BASE_URL || '') + '/oauth2/token',
          new URLSearchParams({
            grant_type: 'refresh_token',
            refresh_token: refreshToken,
            client_id: 'default_client_id' // 根据您的配置，可能需要动态获取或从环境变量读取
          }),
          {
            headers: {
              'Content-Type': 'application/x-www-form-urlencoded'
            }
          }
        );

        const newTokens = response.data;
        
        // 保存新的 Tokens
        localStorage.setItem('access_token', newTokens.access_token);
        if (newTokens.refresh_token) {
          localStorage.setItem('refresh_token', newTokens.refresh_token);
        }
        
        if (newTokens.expires_in) {
          const expiresAt = Date.now() + (newTokens.expires_in * 1000);
          localStorage.setItem('token_expires_at', expiresAt.toString());
        }

        // 重新处理队列中的请求
        processQueue(null, newTokens.access_token);
        
        // 重新发起当前由于 401 失败的请求
        originalRequest.headers['Authorization'] = `Bearer ${newTokens.access_token}`;
        isRefreshing = false;
        
        return apiClient(originalRequest);
        
      } catch (refreshError) {
        // 刷新 Token 失败（可能已过期或被撤销）
        processQueue(refreshError, null);
        isRefreshing = false;
        clearTokens();
        // 重定向到登录页
        router.push('/');
        return Promise.reject(refreshError);
      }
    }

    return Promise.reject(error);
  }
);

function clearTokens() {
  localStorage.removeItem('access_token');
  localStorage.removeItem('refresh_token');
  localStorage.removeItem('token_expires_at');
  localStorage.removeItem('token_scope');
  localStorage.removeItem('user_info');
}

export default apiClient;
