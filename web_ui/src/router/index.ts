import { createRouter, createWebHistory } from 'vue-router'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/login', name: 'login', component: () => import('../views/Login.vue') },
    {
      path: '/',
      component: () => import('../layouts/MainLayout.vue'),
      redirect: '/dashboard',
      children: [
        { path: 'dashboard', name: 'dashboard', component: () => import('../views/Dashboard.vue') },
        { path: 'users', name: 'users', component: () => import('../views/Users.vue') },
        { path: 'rules', name: 'rules', component: () => import('../views/Rules.vue') },
        { path: 'v2rayn', name: 'v2rayn', component: () => import('../views/V2rayN.vue') },
        { path: 'settings', name: 'settings', component: () => import('../views/Settings.vue') },
        { path: 'logs', name: 'logs', component: () => import('../views/Logs.vue') },
      ]
    }
  ]
})

router.beforeEach((to, _from, next) => {
  if (to.name !== 'login' && !localStorage.getItem('token')) {
    next({ name: 'login' })
  } else {
    next()
  }
})

export default router
